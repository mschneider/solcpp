#pragma once

#include <iostream>
#include <fstream>
#include <string>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include "base58.hpp"
#include "base64.hpp"

namespace solana
{
  using json = nlohmann::json;

  const std::string NATIVE_MINT = "So11111111111111111111111111111111111111112";
  const std::string MEMO_PROGRAM_ID = "MemoSq4gqABAXKb96qnH8TysNcWxMyWCqXgDLGmfcHr";
  const std::string MAINNET_BETA = "https://api.mainnet-beta.solana.com";

  struct PublicKey
  {
    static const auto SIZE = crypto_sign_PUBLICKEYBYTES;
    typedef std::array<uint8_t, SIZE> array_t;

    array_t data;

    static PublicKey empty()
    {
      return {};
    }

    static PublicKey fromBase58(const std::string &b58)
    {
      PublicKey result = {};
      size_t decodedSize = SIZE;
      const auto ok = b58tobin(result.data.data(), &decodedSize, b58.c_str(), 0);
      if (!ok)
        throw std::runtime_error("invalid base58 '" + b58 + "'");
      if (decodedSize != SIZE)
        throw std::runtime_error("not a valid PublicKey '" + std::to_string(decodedSize) + " != " + std::to_string(SIZE) + "'");
      return result;
    }

    bool operator==(const PublicKey &other) const
    {
      return data == other.data;
    }

    std::string toBase58() const
    {
      return b58encode(data);
    }
  };

  struct PrivateKey
  {
    static const size_t SIZE = crypto_sign_SECRETKEYBYTES;
    typedef std::array<uint8_t, SIZE> array_t;

    array_t data;

    std::vector<uint8_t> signMessage(const std::vector<uint8_t> message) const
    {
      uint8_t sig[crypto_sign_BYTES];
      unsigned long long sigSize;
      if (0 != crypto_sign_detached(sig, &sigSize, message.data(), message.size(), data.data()))
        throw std::runtime_error("could not sign tx with private key");
      return std::vector<uint8_t>(sig, sig + sigSize);
    }
  };

  struct Keypair
  {
    PublicKey publicKey;
    PrivateKey privateKey;

    static Keypair fromFile(const std::string &path)
    {
      Keypair result = {};
      std::ifstream fileStream(path);
      std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
      result.privateKey.data = json::parse(fileContent);
      crypto_sign_ed25519_sk_to_pk(result.publicKey.data.data(), result.privateKey.data.data());
      return result;
    }
  };

  struct AccountMeta
  {
    PublicKey pubkey;
    bool isSigner;
    bool isWritable;

    bool operator<(const AccountMeta &other) const
    {
      return (isSigner > other.isSigner) || (isWritable > other.isWritable);
    }
  };

  struct Instruction
  {
    PublicKey programId;
    std::vector<AccountMeta> accounts;
    std::vector<uint8_t> data;
  };

  void pushCompactU16(uint16_t num, std::vector<uint8_t> &buffer)
  {
    buffer.push_back(num & 0x7f);
    num >>= 7;
    if (num == 0)
      return;

    buffer.back() |= 0x80;
    buffer.push_back(num & 0x7f);
    num >>= 7;
    if (num == 0)
      return;

    buffer.back() |= 0x80;
    buffer.push_back(num & 0x3);
  };

  void pushCompactVecU8(const std::vector<uint8_t> &vec, std::vector<uint8_t> &buffer)
  {
    pushCompactU16(vec.size(), buffer);
    buffer.insert(buffer.end(), vec.begin(), vec.end());
  }

  struct CompiledInstruction
  {
    uint8_t programIdIndex;
    std::vector<uint8_t> accountIndices;
    std::vector<uint8_t> data;

    static CompiledInstruction fromInstruction(const Instruction &ix, const std::vector<PublicKey> &accounts)
    {
      const auto programIdIt = std::find(accounts.begin(), accounts.end(), ix.programId);
      const auto programIdIndex = static_cast<uint8_t>(programIdIt - accounts.begin());
      std::vector<uint8_t> accountIndices;
      for (const auto &account : ix.accounts)
      {
        const auto accountPkIt = std::find(accounts.begin(), accounts.end(), account.pubkey);
        accountIndices.push_back(accountPkIt - accounts.begin());
      }
      return {programIdIndex, accountIndices, ix.data};
    }

    void serializeTo(std::vector<uint8_t> &buffer) const
    {
      buffer.push_back(programIdIndex);
      pushCompactVecU8(accountIndices, buffer);
      pushCompactVecU8(data, buffer);
    }
  };

  struct CompiledTransaction
  {
    PublicKey recentBlockhash;
    std::vector<PublicKey> accounts;
    std::vector<CompiledInstruction> instructions;
    uint8_t requiredSignatures;
    uint8_t readOnlySignedAccounts;
    uint8_t readOnlyUnsignedAccounts;

    static CompiledTransaction fromInstructions(const std::vector<Instruction> &instructions, const PublicKey &payer, const PublicKey &blockhash)
    {
      // collect all program ids and accounts including the payer
      std::vector<AccountMeta> allMetas = {{payer, true, true}};
      for (const auto &instruction : instructions)
      {
        allMetas.insert(allMetas.end(), instruction.accounts.begin(), instruction.accounts.end());
        allMetas.push_back({instruction.programId, false, false});
      }

      // merge account metas referencing the same acc/pubkey, assign maximum privileges
      std::vector<AccountMeta> uniqueMetas;
      for (const auto &meta : allMetas)
      {
        auto dup = std::find_if(uniqueMetas.begin(), uniqueMetas.end(), [&meta](const auto &u)
                                { return u.pubkey == meta.pubkey; });

        if (dup == uniqueMetas.end())
        {
          uniqueMetas.push_back(meta);
        }
        else
        {
          dup->isSigner |= meta.isSigner;
          dup->isWritable |= meta.isWritable;
        }
      }

      // sort using operator< to establish order: signer+writable, signers, writables, others
      std::sort(uniqueMetas.begin(), uniqueMetas.end());

      uint8_t requiredSignatures = 0;
      uint8_t readOnlySignedAccounts = 0;
      uint8_t readOnlyUnsignedAccounts = 0;
      std::vector<PublicKey> accounts;
      for (const auto &meta : uniqueMetas)
      {
        accounts.push_back(meta.pubkey);
        if (meta.isSigner)
        {
          requiredSignatures++;
          if (!meta.isWritable)
          {
            readOnlySignedAccounts++;
          }
        }
        else if (!meta.isWritable)
        {
          readOnlyUnsignedAccounts++;
        }
      }

      // dictionary encode individual instructions
      std::vector<CompiledInstruction> cixs;
      for (const auto &instruction : instructions)
      {
        cixs.push_back(CompiledInstruction::fromInstruction(instruction, accounts));
      }
      return {blockhash, accounts, cixs, requiredSignatures, readOnlySignedAccounts, readOnlyUnsignedAccounts};
    };

    void serializeTo(std::vector<uint8_t> &buffer) const
    {
      buffer.push_back(requiredSignatures);
      buffer.push_back(readOnlySignedAccounts);
      buffer.push_back(readOnlyUnsignedAccounts);

      pushCompactU16(accounts.size(), buffer);
      for (const auto &account : accounts)
      {
        buffer.insert(buffer.end(), account.data.begin(), account.data.end());
      }

      buffer.insert(buffer.end(), recentBlockhash.data.begin(), recentBlockhash.data.end());

      pushCompactU16(instructions.size(), buffer);
      for (const auto &instruction : instructions)
      {
        instruction.serializeTo(buffer);
      }
    };
  };

  namespace rpc
  {
    using json = nlohmann::json;

    json jsonRequest(const std::string &method, const json &params = nullptr)
    {
      json req = {
          {"jsonrpc", "2.0"},
          {"id", 1},
          {"method", method}};
      if (params != nullptr)
        req["params"] = params;
      return req;
    }

    json accountSubscribeRequest(const std::string &account, const std::string &commitment = "processed", const std::string &encoding = "base64")
    {
      const json params = {
          account,
          {{"commitment", commitment}, {"encoding", encoding}}};

      return jsonRequest("accountSubscribe", params);
    }

    json getAccountInfoRequest(const std::string &account, const std::string &encoding = "base64")
    {
      const json params = {
          account,
          {{"encoding", encoding}}};

      return jsonRequest("getAccountInfo", params);
    }

    template <typename T>
    T getAccountInfo(const std::string &endpoint, const std::string &account)
    {
      const json req = getAccountInfoRequest(account);
      cpr::Response r = cpr::Post(cpr::Url{endpoint},
                                  cpr::Body{req.dump()},
                                  cpr::Header{{"Content-Type", "application/json"}});
      if (r.status_code != 200)
        throw std::runtime_error("unexpected status_code " + std::to_string(r.status_code));

      json res = json::parse(r.text);
      const std::string encoded = res["result"]["value"]["data"][0];
      const std::string decoded = b64decode(encoded);
      if (decoded.size() != sizeof(T))
        throw std::runtime_error("invalid response length " + std::to_string(decoded.size()) + " expected " + std::to_string(sizeof(T)));

      T result;
      memcpy(&result, decoded.data(), sizeof(T));
      return result;
    }

    json getRecentBlockhashRequest(const std::string &commitment = "finalized")
    {
      const json params = {
          {{"commitment", commitment}}};

      return jsonRequest("getRecentBlockhash", params);
    }

    PublicKey getRecentBlockhash(const std::string &endpoint, const std::string &commitment = "finalized")
    {
      const json req = getRecentBlockhashRequest(commitment);
      cpr::Response r = cpr::Post(cpr::Url{endpoint},
                                  cpr::Body{req.dump()},
                                  cpr::Header{{"Content-Type", "application/json"}});
      if (r.status_code != 200)
        throw std::runtime_error("unexpected status_code " + std::to_string(r.status_code));

      json res = json::parse(r.text);
      const std::string encoded = res["result"]["value"]["blockhash"];
      return PublicKey::fromBase58(encoded);
    }

    json getSignatureStatuses(const std::vector<std::string> &signatures, bool searchTransactionHistory = false)
    {
      const json params = {
          signatures,
          {{"searchTransactionHistory", searchTransactionHistory}}};

      return jsonRequest("getSignatureStatuses", params);
    }

    json sendTransactionRequest(const std::string &transaction, const std::string &encoding = "base58", bool skipPreflight = false, const std::string &preflightCommitment = "finalized")
    {
      const json params = {
          transaction,
          {{"encoding", encoding}, {"skipPreflight", skipPreflight}, {"preflightCommitment", preflightCommitment}}};

      return jsonRequest("sendTransaction", params);
    }

    std::string signAndSendTransaction(const std::string &endpoint, const Keypair &keypair, const CompiledTransaction &tx, bool skipPreflight = false, const std::string &preflightCommitment = "finalized")
    {
      std::vector<uint8_t> txBody;
      tx.serializeTo(txBody);

      const auto signature = keypair.privateKey.signMessage(txBody);
      const auto b58Sig = b58encode(std::string(signature.begin(), signature.end()));

      std::vector<uint8_t> signedTx;
      pushCompactU16(1, signedTx);
      signedTx.insert(signedTx.end(), signature.begin(), signature.end());
      signedTx.insert(signedTx.end(), txBody.begin(), txBody.end());

      const auto b64tx = b64encode(std::string(signedTx.begin(), signedTx.end()));
      const json req = solana::rpc::sendTransactionRequest(b64tx, "base64", skipPreflight, preflightCommitment);
      const std::string jsonSerialized = req.dump();

      cpr::Response r = cpr::Post(cpr::Url{endpoint},
                                  cpr::Body{jsonSerialized},
                                  cpr::Header{{"Content-Type", "application/json"}});
      if (r.status_code != 200)
        throw std::runtime_error("unexpected status_code " + std::to_string(r.status_code));

      json res = json::parse(r.text);
      if (b58Sig != res["result"])
        throw std::runtime_error("could not submit tx: " + r.text);

      return b58Sig;
    }

  }
}