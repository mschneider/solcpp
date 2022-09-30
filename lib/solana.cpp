#include <cpr/cpr.h>
#include <sodium.h>

#include <solana.hpp>

namespace solana {
namespace rpc {
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
json Connection::getAccountInfoRequest(const std::string &account,
                                       const std::string &encoding,
                                       const size_t offset,
                                       const size_t length) {
  json params = {account};
  json options = {{"encoding", encoding}};
  if (offset && length) {
    json dataSlice = {"dataSlice", {{"offset", offset}, {"length", length}}};
    options.emplace(dataSlice);
  }
  params.emplace_back(options);

  return jsonRequest("getAccountInfo", params);
}
json Connection::getMultipleAccountsRequest(
    const std::vector<std::string> &accounts, const std::string &encoding,
    const size_t offset, const size_t length) {
  json pubKeys = json::array();
  for (auto &account : accounts) {
    pubKeys.emplace_back(account);
  }
  json params = {};
  params.emplace_back(pubKeys);
  json options = {{"encoding", encoding}};
  if (offset && length) {
    json dataSlice = {"dataSlice", {{"offset", offset}, {"length", length}}};
    options.emplace(dataSlice);
  }
  params.emplace_back(options);

  return jsonRequest("getMultipleAccounts", params);
}
json Connection::getBlockhashRequest(const std::string &commitment,
                                     const std::string &method) {
  const json params = {{{"commitment", commitment}}};

  return jsonRequest(method, params);
}

json Connection::sendTransactionRequest(
    const std::string &transaction, const std::string &encoding,
    bool skipPreflight, const std::string &preflightCommitment) {
  const json params = {transaction,
                       {{"encoding", encoding},
                        {"skipPreflight", skipPreflight},
                        {"preflightCommitment", preflightCommitment}}};

  return jsonRequest("sendTransaction", params);
}

json Connection::sendAirdropRequest(
  const std::string &account,uint64_t lamports
){

  const json params = {account, lamports};

  return jsonRequest("requestAirdrop", params);
  
}




///
/// 2. Invoke RPC endpoints
///
PublicKey Connection::getRecentBlockhash_DEPRECATED(
    const std::string &commitment) {
  const json req = getBlockhashRequest(commitment);
  cpr::Response r =
      cpr::Post(cpr::Url{rpc_url_}, cpr::Body{req.dump()},
                cpr::Header{{"Content-Type", "application/json"}});
  if (r.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(r.status_code));
  json res = json::parse(r.text);
  const std::string encoded = res["result"]["value"]["blockhash"];
  return PublicKey::fromBase58(encoded);
}
Blockhash Connection::getLatestBlockhash(const std::string &commitment) {
  const json req = getBlockhashRequest(commitment, "getLatestBlockhash");
  cpr::Response r =
      cpr::Post(cpr::Url{rpc_url_}, cpr::Body{req.dump()},
                cpr::Header{{"Content-Type", "application/json"}});
  if (r.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(r.status_code));
  json res = json::parse(r.text);
  const std::string encoded = res["result"]["value"]["blockhash"];
  const uint64_t lastValidBlockHeight =
      static_cast<uint64_t>(res["result"]["value"]["lastValidBlockHeight"]);
  return {PublicKey::fromBase58(encoded), lastValidBlockHeight};
}

uint64_t Connection::getBlockHeight(const std::string &commitment) {
  const json req = getBlockhashRequest(commitment, "getBlockHeight");
  cpr::Response r =
      cpr::Post(cpr::Url{rpc_url_}, cpr::Body{req.dump()},
                cpr::Header{{"Content-Type", "application/json"}});
  if (r.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(r.status_code));
  json res = json::parse(r.text);
  const uint64_t blockHeight = res["result"];
  return blockHeight;
}
json Connection::getSignatureStatuses(
    const std::vector<std::string> &signatures, bool searchTransactionHistory) {
  const json params = {
      signatures, {{"searchTransactionHistory", searchTransactionHistory}}};

  return jsonRequest("getSignatureStatuses", params);
}

std::string Connection::signAndSendTransaction(
    const Keypair &keypair, const CompiledTransaction &tx, bool skipPreflight,
    const std::string &preflightCommitment) {
  std::vector<uint8_t> txBody;
  tx.serializeTo(txBody);
  const auto signature = keypair.privateKey.signMessage(txBody);
  const auto b58Sig =
      b58encode(std::string(signature.begin(), signature.end()));

  std::vector<uint8_t> signedTx;
  solana::CompactU16::encode(1, signedTx);
  signedTx.insert(signedTx.end(), signature.begin(), signature.end());
  signedTx.insert(signedTx.end(), txBody.begin(), txBody.end());

  const auto b64tx = b64encode(std::string(signedTx.begin(), signedTx.end()));
  const json req = sendTransactionRequest(b64tx, "base64", skipPreflight,
                                          preflightCommitment);
  const std::string jsonSerialized = req.dump();

  cpr::Response r =
      cpr::Post(cpr::Url{rpc_url_}, cpr::Body{jsonSerialized},
                cpr::Header{{"Content-Type", "application/json"}});
  if (r.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(r.status_code));

  json res = json::parse(r.text);
  if (b58Sig != res["result"])
    throw std::runtime_error("could not submit tx: " + r.text);

  return b58Sig;
}

std::string Connection::sendRawTransaction(
    const Keypair &keypair, std::vector<uint8_t> &tx, bool skipPreflight,
    const std::string &preflightCommitment) {
  const auto signature = keypair.privateKey.signMessage(tx);
  const auto b58Sig = b58encode(std::string(signature.begin(), signature.end()));

  std::vector<uint8_t> signedTx;
  solana::CompactU16::encode(1, signedTx);
  signedTx.insert(signedTx.end(), signature.begin(), signature.end());
  signedTx.insert(signedTx.end(), tx.begin(), tx.end());

  const auto b64tx = b64encode(std::string(signedTx.begin(), signedTx.end()));
  const json req = sendTransactionRequest(b64tx, "base64", skipPreflight,
                                          preflightCommitment);
  const std::string jsonSerialized = req.dump();

  cpr::Response r =
      cpr::Post(cpr::Url{rpc_url_}, cpr::Body{jsonSerialized},
                cpr::Header{{"Content-Type", "application/json"}});
  if (r.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(r.status_code));

  json res = json::parse(r.text);
  if (b58Sig != res["result"])
    throw std::runtime_error("could not submit tx: " + r.text);

  return b58Sig;
}


std::string Connection::sendTransaction(const std::string &transaction,bool skipPreflight,
    const std::string &preflightCommitment){

    const json req=sendTransactionRequest(transaction,"base64",skipPreflight,preflightCommitment);

      cpr::Response r =
      cpr::Post(cpr::Url{rpc_url_}, cpr::Body{req.dump()},
                cpr::Header{{"Content-Type", "application/json"}});
  if (r.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(r.status_code));
  
  json res=r.text;
  return res["result"];
}

std::string Connection::requestAirdrop(const PublicKey &pubkey,uint64_t lamports,std::string url){

  const json req = sendAirdropRequest(pubkey.toBase58(),lamports);
  cpr::Response r =
      cpr::Post(cpr::Url{url}, cpr::Body{req.dump()},
                cpr::Header{{"Content-Type", "application/json"}});
  //json res = json::parse(r.text);
  std::cout<<pubkey.toBase58()<<r.status_code << std::endl;
  if (r.status_code != 200)
    throw std::runtime_error("unexpected status_code " +
                             std::to_string(r.status_code));

  
  return r.text;

}

}  // namespace rpc

}  // namespace solana