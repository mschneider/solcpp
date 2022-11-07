#include <cpr/cpr.h>
#include <sodium.h>

#include <chrono>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ostream>
#include <solana.hpp>
#include <string>
#include <thread>
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

void to_json(json &j, const PublicKey &key) { j = key.toBase58(); }

void to_json(json &j, const GetSlotConfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.minContextSlot.has_value()) {
    j["minContextSlot"] = config.minContextSlot.value();
  }
}

void from_json(const json &j, PublicKey &key) {
  key = PublicKey::fromBase58(j);
}

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

uint64_t trailingZeros(uint64_t n) {
  uint64_t trailingsZeros = 0;
  while (n > 1) {
    n /= 2;
    trailingsZeros++;
  }
  return trailingsZeros;
}

uint64_t nextPowerOfTwo(uint64_t n) {
  if (n == 0) {
    return 1;
  }
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 32;
  return n + 1;
}
///
/// Version
void from_json(const json &j, Version &version) {
  version.feature_set = j["feature-set"];
  version.solana_core = j["solana-core"];
}
// EpochSchedule
void from_json(const json &j, EpochSchedule &epochschedule) {
  epochschedule.firstNormalEpoch = j["firstNormalEpoch"];
  epochschedule.firstNormalSlot = j["firstNormalSlot"];
  epochschedule.leaderScheduleSlotOffset = j["leaderScheduleSlotOffset"];
  epochschedule.slotsPerEpoch = j["slotsPerEpoch"];
  epochschedule.warmup = j["warmup"];
}

void from_json(const json &j, EpochInfo &epochinfo) {
  epochinfo.absoluteSlot = j["absoluteSlot"];
  epochinfo.blockHeight = j["blockHeight"];
  epochinfo.epoch = j["epoch"];
  epochinfo.slotIndex = j["slotIndex"];
  epochinfo.slotsInEpoch = j["slotsInEpoch"];
  epochinfo.transactionCount = j["transactionCount"];
}
///
/// AccountMeta
bool AccountMeta::operator<(const AccountMeta &other) const {
  return (isSigner > other.isSigner) || (isWritable > other.isWritable);
}

///
/// TransactionReturnData

void from_json(const json &j, TransactionReturnData &data) {
  data.programId = j["programId"];
  data.data = j["data"][0];
}

///
/// SimulatedTransactionResponse
void from_json(const json &j, SimulatedTransactionResponse &res) {
  if (!j["err"].is_null()) {
    res.err = std::optional{j["err"]};
  }
  if (!j["accounts"].is_null()) {
    res.accounts = std::optional<std::vector<std::string>>{j["accounts"]};
  }
  if (!j["logs"].is_null()) {
    res.logs = std::optional<std::vector<std::string>>{j["logs"]};
  }
  if (!j["unitsConsumed"].is_null()) {
    res.unitsConsumed = std::optional{j["unitsConsumed"]};
  }
  if (j.contains("returnData") && !j["returnData"].is_null()) {
    res.returnData = std::optional<TransactionReturnData>{j["returnData"]};
  }
}

void to_json(json &j, const GetStakeActivationConfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.epoch.has_value()) {
    j["epoch"] = config.epoch.value();
  }
  if (config.minContextSlot.has_value()) {
    j["minContextSlot"] = config.minContextSlot.value();
  }
}

void to_json(json &j, const commitmentconfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
}
///
/// SignatureStatus
void to_json(json &j, const SignatureStatus &status) {
  j["slot"] = status.slot;
  if (status.confirmations.has_value()) {
    j["confirmations"] = status.confirmations.value();
  }
  if (status.err.has_value()) {
    j["err"] = status.err.value();
  }
  j["confirmationStatus"] = status.confirmationStatus;
}

void from_json(const json &j, SignatureStatus &res) {
  res.slot = j["slot"];
  if (!j["confirmations"].is_null()) {
    res.confirmations = std::optional{j["confirmations"]};
  }
  if (!j["err"].is_null()) {
    res.err = std::optional{j["err"]};
  }
  res.confirmationStatus = j["confirmationStatus"];
}

void from_json(const json &j, StakeActivation &stakeactivation) {
  stakeactivation.active = j["active"];
  stakeactivation.inactive = j["inactive"];
  stakeactivation.state = j["state"];
}

void from_json(const json &j, InflationGovernor &inflationgovernor) {
  inflationgovernor.foundation = j["foundation"];
  inflationgovernor.foundationTerm = j["foundationTerm"];
  inflationgovernor.initial = j["initial"];
  inflationgovernor.taper = j["taper"];
  inflationgovernor.terminal = j["terminal"];
}

void from_json(const json &j, Nodes &nodes) {
  if (!j["featureSet"].is_null()) {
    nodes.featureSet = std::optional{j["featureSet"]};
  }
  if (!j["gossip"].is_null()) {
    nodes.gossip = std::optional{j["gossip"]};
  }
  if (!j["pubkey"].is_null()) {
    nodes.pubkey = std::optional{j["pubkey"]};
  }
  if (!j["rpc"].is_null()) {
    nodes.rpc = std::optional{j["rpc"]};
  }
  if (!j["shredVersion"].is_null()) {
    nodes.shredVersion = std::optional{j["shredVersion"]};
  }
  if (!j["tpu"].is_null()) {
    nodes.tpu = std::optional{j["tpu"]};
  }
  if (!j["version"].is_null()) {
    nodes.version = std::optional{j["version"]};
  }
}

void from_json(const json &j, getFeeForMessageRes &res) {
  if (!j.is_null()) {
    res.value = j;
  }
}

void from_json(const json &j, LargestAccounts &largestaccounts) {
  largestaccounts.address = j["address"];
  largestaccounts.lamports = j["lamports"];
}

void from_json(const json &j,
               RecentPerformanceSamples &recentperformancesamples) {
  recentperformancesamples.numSlots = j["numSlots"];
  recentperformancesamples.numTransactions = j["numTransactions"];
  recentperformancesamples.samplePeriodSecs = j["samplePeriodSecs"];
  recentperformancesamples.slot = j["slot"];
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

///
/// SendTransactionConfig
void to_json(json &j, const SendTransactionConfig &config) {
  j["encoding"] = config.encoding;

  if (config.skipPreflight.has_value()) {
    j["skipPreflight"] = config.skipPreflight.value();
  }
  if (config.preflightCommitment.has_value()) {
    j["preflightCommitment"] = config.preflightCommitment.value();
  }
  if (config.maxRetries.has_value()) {
    j["maxRetries"] = config.maxRetries.value();
  }
  if (config.minContextSlot.has_value()) {
    j["minContextSlot"] = config.minContextSlot.value();
  }
}

///
/// SimulateTransactionConfig
void to_json(json &j, const SimulateTransactionConfig &config) {
  j["encoding"] = BASE64;

  if (config.sigVerify.has_value()) {
    j["sigVerify"] = config.sigVerify.value();
  }
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.replaceRecentBlockhash.has_value()) {
    j["replaceRecentBlockhash"] = config.replaceRecentBlockhash.value();
  }
  if (config.address.has_value()) {
    j["accounts"] = {{"addresses", config.address.value()}};
  }
  if (config.minContextSlot.has_value()) {
    j["minContextSlot"] = config.minContextSlot.value();
  }
}

///
/// GetAccountInfoConfig
void to_json(json &j, const GetAccountInfoConfig &config) {
  j["encoding"] = BASE64;
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.minContextSlot.has_value()) {
    j["minContextSlot"] = config.minContextSlot.value();
  }
  if (config.dataSlice.has_value()) {
    j["dataSlice"] = config.dataSlice.value();
  }
}

///
/// Connection
Connection::Connection(const std::string &rpc_url)
    : rpc_url_(std::move(rpc_url)) {
  auto sodium_result = sodium_init();
  if (sodium_result < -1)
    throw std::runtime_error("Error initializing sodium: " +
                             std::to_string(sodium_result));
}

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

std::string Connection::signAndSendTransaction(
    const Keypair &keypair, const CompiledTransaction &tx, bool skipPreflight,
    const Commitment &preflightCommitment) const {
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
  const json params = {b64Tx, config};
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
  const json params = {b64Tx, config};
  const auto reqJson = jsonRequest("simulateTransaction", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson)["value"];
}

std::string Connection::requestAirdrop(const PublicKey &pubkey,
                                       uint64_t lamports) const {
  // create request
  const json params = {pubkey.toBase58(), lamports};
  const json reqJson = jsonRequest("requestAirdrop", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson);
}

uint64_t Connection::getBalance(const PublicKey &pubkey) const {
  // create request
  const json params = {pubkey.toBase58()};
  const json reqJson = jsonRequest("getBalance", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson)["value"];
}

PublicKey Connection::getRecentBlockhash(const Commitment &commitment) const {
  // create request
  const json params = {{{"commitment", commitment}}};
  const json reqJson = jsonRequest("getRecentBlockhash", params);
  // send jsonRpc request and create PublicKey from base58 encoded blockhash
  return PublicKey::fromBase58(
      sendJsonRpcRequest(reqJson)["value"]["blockhash"]);
}

Blockhash Connection::getLatestBlockhash(const Commitment &commitment) const {
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

uint64_t Connection::getBlockHeight(const Commitment &commitment) const {
  // create request
  const json params = {{{"commitment", commitment}}};
  const json reqJson = jsonRequest("getBlockHeight", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson);
}

RpcResponseAndContext<std::vector<std::optional<SignatureStatus>>>
Connection::getSignatureStatuses(const std::vector<std::string> &signatures,
                                 bool searchTransactionHistory) const {
  // create request
  const json params = {
      signatures, {{"searchTransactionHistory", searchTransactionHistory}}};
  const auto reqJson = jsonRequest("getSignatureStatuses", params);
  // send jsonRpc request
  const json res = sendJsonRpcRequest(reqJson);
  // parse response and handle null status
  const std::vector<json> value = res["value"];
  std::vector<std::optional<SignatureStatus>> status_list;
  status_list.reserve(value.size());

  for (const json &status : value) {
    if (status.is_null()) {
      status_list.push_back(std::nullopt);
    } else {
      status_list.push_back(std::optional{status});
    }
  }

  return {res["context"], status_list};
}

RpcResponseAndContext<std::optional<SignatureStatus>>
Connection::getSignatureStatus(const std::string &signature,
                               bool searchTransactionHistory) const {
  const auto res = getSignatureStatuses({signature}, searchTransactionHistory);
  return {res.context, res.value[0]};
}

bool Connection::confirmTransaction(std::string transactionSignature,
                                    Commitment confirmLevel,
                                    uint16_t retries) const {
  const auto timeoutBlockheight =
      getLatestBlockhash(confirmLevel).lastValidBlockHeight +
      solana::MAXIMUM_NUMBER_OF_BLOCKS_FOR_TRANSACTION;
  while (retries > 0) {
    auto currentBlockheight = getBlockHeight(confirmLevel);
    const auto res = getSignatureStatus(transactionSignature, true).value;
    if (timeoutBlockheight <= currentBlockheight)
      throw std::runtime_error("Transaction timeout");
    if (res.has_value() && res.value().confirmationStatus == confirmLevel) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    retries--;
  }
  return false;
}

Version Connection::getVersion() const {
  // create request
  json params = {};
  const json reqJson = jsonRequest("getVersion", params);
  // send jsonRpc request
  return sendJsonRpcRequest(reqJson);
}

uint64_t Connection::getFirstAvailableBlock() const {
  json params = {};
  const json reqJson = jsonRequest("getFirstAvailableBlock", params);
  return sendJsonRpcRequest(reqJson);
}

uint64_t Connection::getSlot(const GetSlotConfig &config) const {
  const json params = {config};
  const json reqJson = jsonRequest("getSlot", params);
  return sendJsonRpcRequest(reqJson);
}

uint64_t Connection::minimumLedgerSlot() const {
  const json params = {};
  const json reqJson = jsonRequest("minimumLedgerSlot", params);
  return sendJsonRpcRequest(reqJson);
}

std::string Connection::getGenesisHash() const {
  const json params = {};
  const json reqJson = jsonRequest("getGenesisHash", params);
  return sendJsonRpcRequest(reqJson);
}

std::string Connection::getSlotLeader(const GetSlotConfig &config) const {
  const json params = {config};
  const json reqJson = jsonRequest("getSlotLeader", params);
  return sendJsonRpcRequest(reqJson);
}

EpochSchedule Connection::getEpochSchedule() const {
  json params = {};
  const json reqJson = jsonRequest("getEpochSchedule", params);
  return sendJsonRpcRequest(reqJson);
}

StakeActivation Connection::getStakeActivation(
    const PublicKey &pubkey, const GetStakeActivationConfig &config) const {
  const json params = {pubkey, config};
  const json reqJson = jsonRequest("getStakeActivation", params);
  return sendJsonRpcRequest(reqJson);
}

InflationGovernor Connection::getInflationGovernor(
    const commitmentconfig &config) const {
  const json params = {};
  const json reqJson = jsonRequest("getInflationGovernor", params);
  return sendJsonRpcRequest(reqJson);
}

uint64_t Connection::getTransactionCount(const GetSlotConfig &config) const {
  const json params = {config};
  const json reqJson = jsonRequest("getTransactionCount", params);
  return sendJsonRpcRequest(reqJson);
}

EpochInfo Connection::getEpochInfo(const GetSlotConfig &config) const {
  const json params = {config};
  const json reqJson = jsonRequest("getEpochInfo", params);
  return sendJsonRpcRequest(reqJson);
}

uint64_t Connection::getMinimumBalanceForRentExemption(
    const std::size_t dataLength, const commitmentconfig &config) const {
  const json params = {dataLength, config};
  const json reqJson = jsonRequest("getMinimumBalanceForRentExemption", params);
  return sendJsonRpcRequest(reqJson);
}

uint64_t Connection::getBlockTime(const uint64_t slot) const {
  const json params = {slot};
  const json reqJson = jsonRequest("getBlockTime", params);
  return sendJsonRpcRequest(reqJson);
}

std::vector<Nodes> Connection::getClusterNodes() const {
  const json params = {};
  const json reqJson = jsonRequest("getClusterNodes", params);
  return sendJsonRpcRequest(reqJson);
}

getFeeForMessageRes Connection::getFeeForMessage(
    const std::string message, const GetSlotConfig &config) const {
  const json params = {message, config};
  const json reqJson = jsonRequest("getFeeForMessage", params);
  return sendJsonRpcRequest(reqJson)["value"];
}

RpcResponseAndContext<std::vector<LargestAccounts>>
Connection::getLargestAccounts() const {
  const json params = {};
  const auto reqJson = jsonRequest("getLargestAccounts", params);
  const json res = sendJsonRpcRequest(reqJson);
  const std::vector<json> value = res["value"];
  std::vector<LargestAccounts> accounts_list;
  accounts_list.reserve(value.size());
  for (const json &account : value) {
    accounts_list.push_back(account);
  }
  return {res["context"], accounts_list};
}

std::vector<RecentPerformanceSamples> Connection::getRecentPerformanceSamples(
    std::size_t limit) const {
  const json params = {limit};
  const auto reqJson = jsonRequest("getRecentPerformanceSamples", params);
  const json res = sendJsonRpcRequest(reqJson);
  const std::vector<json> value = res;
  std::vector<RecentPerformanceSamples> samples_list;
  samples_list.reserve(value.size());
  for (const json &sample : value) {
    samples_list.push_back(sample);
  }
  return samples_list;
}

}  // namespace rpc

namespace subscription {}  // namespace subscription

}  // namespace solana
