#include <cpr/cpr.h>
#include <sodium.h>

#include <cstdint>
#include <iterator>
#include <optional>
#include <ostream>
#include <solana.hpp>
#include <string>
#include <vector>

#include "base64.hpp"
#include "cpr/api.h"
#include "cpr/body.h"
#include "cpr/cprtypes.h"

namespace solana {

///
/// PublicKey
PublicKey PublicKey::empty() { return {}; }

PublicKey PublicKey::fromBase58(const std::string &b58) {
  PublicKey result = {};
  size_t decodedSize = SIZE;
  const auto ok = b58tobin(result.data.data(), &decodedSize, b58.c_str(), 0);
  if (!ok) throw std::runtime_error("invalid base58 '" + b58 + "'");
  if (decodedSize != SIZE)
    throw std::runtime_error("not a valid PublicKey '" +
                             std::to_string(decodedSize) +
                             " != " + std::to_string(SIZE) + "'");
  return result;
}

bool PublicKey::operator==(const PublicKey &other) const {
  return data == other.data;
}

std::string PublicKey::toBase58() const { return b58encode(data); }

///
/// PrivateKey
std::vector<uint8_t> PrivateKey::signMessage(
    const std::vector<uint8_t> message) const {
  uint8_t sig[crypto_sign_BYTES];
  unsigned long long sigSize;
  if (0 != crypto_sign_detached(sig, &sigSize, message.data(), message.size(),
                                data.data()))
    throw std::runtime_error("could not sign tx with private key");
  return std::vector<uint8_t>(sig, sig + sigSize);
}

///
/// Keypair
Keypair Keypair::fromFile(const std::string &path) {
  Keypair result = {};
  std::ifstream fileStream(path);
  std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
  result.privateKey.data = json::parse(fileContent);
  crypto_sign_ed25519_sk_to_pk(result.publicKey.data.data(),
                               result.privateKey.data.data());
  return result;
}

///
/// AccountMeta
bool AccountMeta::operator<(const AccountMeta &other) const {
  return (isSigner > other.isSigner) || (isWritable > other.isWritable);
}

///
/// CompactU16
namespace CompactU16 {
void encode(uint16_t num, std::vector<uint8_t> &buffer) {
  buffer.push_back(num & 0x7f);
  num >>= 7;
  if (num == 0) return;

  buffer.back() |= 0x80;
  buffer.push_back(num & 0x7f);
  num >>= 7;
  if (num == 0) return;

  buffer.back() |= 0x80;
  buffer.push_back(num & 0x3);
};

void encode(const std::vector<uint8_t> &vec, std::vector<uint8_t> &buffer) {
  encode(vec.size(), buffer);
  buffer.insert(buffer.end(), vec.begin(), vec.end());
}
}  // namespace CompactU16

///
/// CompiledInstruction
CompiledInstruction CompiledInstruction::fromInstruction(
    const Instruction &ix, const std::vector<PublicKey> &accounts) {
  const auto programIdIt =
      std::find(accounts.begin(), accounts.end(), ix.programId);
  const auto programIdIndex =
      static_cast<uint8_t>(programIdIt - accounts.begin());
  std::vector<uint8_t> accountIndices;
  for (const auto &account : ix.accounts) {
    const auto accountPkIt =
        std::find(accounts.begin(), accounts.end(), account.pubkey);
    accountIndices.push_back(accountPkIt - accounts.begin());
  }
  return {programIdIndex, accountIndices, ix.data};
}

void CompiledInstruction::serializeTo(std::vector<uint8_t> &buffer) const {
  buffer.push_back(programIdIndex);
  solana::CompactU16::encode(accountIndices, buffer);
  solana::CompactU16::encode(data, buffer);
}

///
/// CompiledTransaction
CompiledTransaction CompiledTransaction::fromInstructions(
    const std::vector<Instruction> &instructions, const PublicKey &payer,
    const Blockhash &blockhash) {
  // collect all program ids and accounts including the payer
  std::vector<AccountMeta> allMetas = {{payer, true, true}};
  for (const auto &instruction : instructions) {
    allMetas.insert(allMetas.end(), instruction.accounts.begin(),
                    instruction.accounts.end());
    allMetas.push_back({instruction.programId, false, false});
  }

  // merge account metas referencing the same acc/pubkey, assign maximum
  // privileges
  std::vector<AccountMeta> uniqueMetas;
  for (const auto &meta : allMetas) {
    auto dup = std::find_if(
        uniqueMetas.begin(), uniqueMetas.end(),
        [&meta](const auto &u) { return u.pubkey == meta.pubkey; });

    if (dup == uniqueMetas.end()) {
      uniqueMetas.push_back(meta);
    } else {
      dup->isSigner |= meta.isSigner;
      dup->isWritable |= meta.isWritable;
    }
  }

  // sort using operator< to establish order: signer+writable, signers,
  // writables, others
  std::sort(uniqueMetas.begin(), uniqueMetas.end());

  uint8_t requiredSignatures = 0;
  uint8_t readOnlySignedAccounts = 0;
  uint8_t readOnlyUnsignedAccounts = 0;
  std::vector<PublicKey> accounts;
  for (const auto &meta : uniqueMetas) {
    accounts.push_back(meta.pubkey);
    if (meta.isSigner) {
      requiredSignatures++;
      if (!meta.isWritable) {
        readOnlySignedAccounts++;
      }
    } else if (!meta.isWritable) {
      readOnlyUnsignedAccounts++;
    }
  }

  // dictionary encode individual instructions
  std::vector<CompiledInstruction> cixs;
  for (const auto &instruction : instructions) {
    cixs.push_back(CompiledInstruction::fromInstruction(instruction, accounts));
  }
  return {blockhash,
          accounts,
          cixs,
          requiredSignatures,
          readOnlySignedAccounts,
          readOnlyUnsignedAccounts};
}

void CompiledTransaction::serializeTo(std::vector<uint8_t> &buffer) const {
  buffer.push_back(requiredSignatures);
  buffer.push_back(readOnlySignedAccounts);
  buffer.push_back(readOnlyUnsignedAccounts);

  solana::CompactU16::encode(accounts.size(), buffer);
  for (const auto &account : accounts) {
    buffer.insert(buffer.end(), account.data.begin(), account.data.end());
  }

  buffer.insert(buffer.end(), recentBlockhash.publicKey.data.begin(),
                recentBlockhash.publicKey.data.end());

  solana::CompactU16::encode(instructions.size(), buffer);
  for (const auto &instruction : instructions) {
    instruction.serializeTo(buffer);
  }
}

std::vector<uint8_t> CompiledTransaction::signTransaction(
    const Keypair &keypair, const std::vector<uint8_t> &tx) {
  // create signature
  const auto signature = keypair.privateKey.signMessage(tx);
  // sign the transaction
  std::vector<uint8_t> signedTx;
  solana::CompactU16::encode(1, signedTx);
  signedTx.insert(signedTx.end(), signature.begin(), signature.end());
  signedTx.insert(signedTx.end(), tx.begin(), tx.end());

  return signedTx;
}

std::vector<uint8_t> CompiledTransaction::sign(const Keypair &keypair) const {
  // serialize transaction
  std::vector<uint8_t> tx;
  serializeTo(tx);
  // sign and base64 encode the transaction
  return signTransaction(keypair, tx);
}

namespace rpc {

json jsonRequest(const std::string &method, const json &params) {
  json req = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", method}};
  if (params != nullptr) req["params"] = params;
  return req;
}

json SendTransactionConfig::toJson() const {
  json value = {{"encoding", encoding}};

  if (skipPreflight.has_value()) {
    value["skipPreflight"] = skipPreflight.value();
  }
  if (preflightCommitment.has_value()) {
    value["preflightCommitment"] = preflightCommitment.value();
  }
  if (maxRetries.has_value()) {
    value["maxRetries"] = maxRetries.value();
  }
  if (minContextSlot.has_value()) {
    value["minContextSlot"] = minContextSlot.value();
  }

  return value;
}

json SimulateTransactionConfig::toJson() const {
  json value = {{"encoding", BASE64}};

  if (sigVerify.has_value()) {
    value["sigVerify"] = sigVerify.value();
  }
  if (commitment.has_value()) {
    value["commitment"] = commitment.value();
  }
  if (replaceRecentBlockhash.has_value()) {
    value["replaceRecentBlockhash"] = replaceRecentBlockhash.value();
  }
  if (address.has_value()) {
    value["accounts"] = {{"addresses", address.value()}};
  }
  if (minContextSlot.has_value()) {
    value["minContextSlot"] = minContextSlot.value();
  }
  return value;
}

///
/// Connection
Connection::Connection(const std::string &rpc_url,
                       const std::string &commitment)
    : rpc_url_(std::move(rpc_url)), commitment_(std::move(commitment)) {
  auto sodium_result = sodium_init();
  if (sodium_result < -1)
    throw std::runtime_error("Error initializing sodium: " +
                             std::to_string(sodium_result));
}

///
/// 1. Build requests
///
json Connection::sendJsonRpcRequest(const json &body) const {
  cpr::Response res =
      cpr::Post(cpr::Url{rpc_url_}, cpr::Body{body.dump()},
                cpr::Header{{"Content-Type", "application/json"}});

  if (res.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(res.status_code));

  const auto resJson = json::parse(res.text);

  if (resJson.contains("error")) {
    throw std::runtime_error(resJson["error"].dump());
  }

  return resJson["result"];
}

///
/// 2. Invoke RPC endpoints
///
std::string Connection::signAndSendTransaction(
    const Keypair &keypair, const CompiledTransaction &tx, bool skipPreflight,
    const std::string &preflightCommitment) {
  const SendTransactionConfig config{skipPreflight, preflightCommitment};
  return sendTransaction(keypair, tx, config);
}

std::string Connection::sendTransaction(
    const Keypair &keypair, const CompiledTransaction &compiledTx,
    const SendTransactionConfig &config) const {
  // sign and encode transaction
  const auto signedTx = compiledTx.sign(keypair);
  const auto b64Tx = b64encode(std::string(signedTx.begin(), signedTx.end()));
  // send jsonRpc request
  return sendEncodedTransaction(b64Tx, config);
}

std::string Connection::sendRawTransaction(
    const std::vector<uint8_t> &signedTx,
    const SendTransactionConfig &config) const {
  // base64 encode transaction
  const auto b64Tx = b64encode(std::string(signedTx.begin(), signedTx.end()));
  // send jsonRpc request
  return sendEncodedTransaction(b64Tx, config);
}

std::string Connection::sendEncodedTransaction(
    const std::string &b64Tx, const SendTransactionConfig &config) const {
  // create request
  const json params = {b64Tx, config.toJson()};
  const json reqJson = jsonRequest("sendTransaction", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson);
}

SimulatedTransactionResponse Connection::simulateTransaction(
    const Keypair &keypair, const CompiledTransaction &compiledTx,
    const SimulateTransactionConfig &config) const {
  // signed and encode transaction
  const auto signedTx = compiledTx.sign(keypair);
  const auto b64Tx = b64encode(std::string(signedTx.begin(), signedTx.end()));
  // create request
  const json params = {b64Tx, config.toJson()};
  const auto reqJson = jsonRequest("simulateTransaction", params);
  const json res = sendJsonRpcRequest(reqJson)["value"];
  std::string err = "";
  std::vector<std::string> accounts;
  std::vector<std::string> logs;
  if (!res["err"].is_null()) {
    err = res["err"];
  }

  if (!res["accounts"].is_null()) {
    accounts = res["accounts"].get<std::vector<std::string>>();
  }
  if (!res["logs"].is_null()) {
    logs = res["logs"].get<std::vector<std::string>>();
  }
  uint64_t unitsConsumed = res["unitsConsumed"];
  // send jsonRpc request
  return {err, accounts, logs, unitsConsumed};
}

std::string Connection::requestAirdrop(const PublicKey &pubkey,
                                       uint64_t lamports) {
  // create request
  const json params = {pubkey.toBase58(), lamports};
  const json reqJson = jsonRequest("requestAirdrop", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson);
}

Balance Connection::getBalance(const PublicKey &pubkey) {
  // create request
  const json params = {pubkey.toBase58()};
  const json reqJson = jsonRequest("getBalance", params);
  auto res = sendJsonRpcRequest(reqJson);
  uint64_t lamports = res["value"];
  // send jsonRpc request
  return {lamports};
}

PublicKey Connection::getRecentBlockhash(const std::string &commitment) {
  // create request
  const json params = {{{"commitment", commitment}}};
  const json reqJson = jsonRequest("getRecentBlockhash", params);
  // send jsonRpc request and create PublicKey from base58 encoded blockhash
  return PublicKey::fromBase58(
      sendJsonRpcRequest(reqJson)["value"]["blockhash"]);
}

Blockhash Connection::getLatestBlockhash(const std::string &commitment) {
  // create request
  const json params = {{{"commitment", commitment}}};
  const json reqJson = jsonRequest("getLatestBlockhash", params);
  // send jsonRpc request
  const json res = sendJsonRpcRequest(reqJson);
  const json value = res["value"];
  // create Blockhash from response
  const PublicKey blockhash = PublicKey::fromBase58(value["blockhash"]);
  const uint64_t lastValidBlockHeight =
      static_cast<uint64_t>(value["lastValidBlockHeight"]);
  return {blockhash, lastValidBlockHeight};
}

uint64_t Connection::getBlockHeight(const std::string &commitment) {
  // create request
  const json params = {{{"commitment", commitment}}};
  const json reqJson = jsonRequest("getBlockHeight", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson);
}

json Connection::getSignatureStatuses(
    const std::vector<std::string> &signatures, bool searchTransactionHistory) {
  const json params = {
      signatures, {{"searchTransactionHistory", searchTransactionHistory}}};

  return jsonRequest("getSignatureStatuses", params);
}

}  // namespace rpc

namespace subscription {}  // namespace subscription

}  // namespace solana
