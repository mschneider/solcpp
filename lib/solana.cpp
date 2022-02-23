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
json Connection::getRecentBlockhashRequest(const std::string &commitment) {
  const json params = {{{"commitment", commitment}}};

  return jsonRequest("getRecentBlockhash", params);
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

///
/// 2. Invoke RPC endpoints
///
PublicKey Connection::getRecentBlockhash(const std::string &commitment) {
  const json req = getRecentBlockhashRequest(commitment);
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
}  // namespace rpc

}  // namespace solana