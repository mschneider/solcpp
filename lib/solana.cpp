#include "solana.hpp"

#include <cpr/cpr.h>
#include <sodium.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <iterator>
#include <nlohmann/json.hpp>
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

void to_json(json &j, const BlockProductionConfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.range.has_value()) {
    j["range"] = config.range.value();
  }
  if (config.identity.has_value()) {
    j["identity"] = config.identity.value();
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

void to_json(json &j, const mintOrProgramIdConfig &config) {
  if (config.mint.has_value()) {
    j["mint"] = config.mint.value();
  } else {
    j["programId"] = config.programId.value();
  }
}

void to_json(json &j, const TokenAccountsByOwnerConfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.encoding.has_value()) {
    j["encoding"] = config.encoding.value();
  }
  if (config.dataSlice.has_value()) {
    j["dataSlice"] = config.dataSlice.value();
  }
  if (config.minContextSlot.has_value()) {
    j["minContextSlot"] = config.minContextSlot.value();
  }
}
void to_json(json &j, const LargestAccountsConfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.filter.has_value()) {
    j["filter"] = config.filter.value();
  }
}

void from_json(const json &j, Supply &supply) {
  supply.circulating = j["circulating"];
  supply.nonCirculating = j["nonCirculating"];
  supply.nonCirculatingAccounts =
      std::optional<std::vector<std::string>>{j["nonCirculatingAccounts"]};
  supply.total = j["total"];
}
void from_json(const json &j, TokenAccountBalance &tokenaccountbalance) {
  tokenaccountbalance.amount = j["amount"];
  tokenaccountbalance.decimals = j["decimals"];
  tokenaccountbalance.uiAmount = j["uiAmount"];
  tokenaccountbalance.uiAmountString = j["uiAmountString"];
}

void from_json(const json &j, Current &current) {
  current.activatedStake = j["activatedStake"];
  current.commission = j["commission"];
  current.epochCredits = j["epochCredits"];
  current.epochVoteAccount = j["epochVoteAccount"];
  current.lastVote = j["lastVote"];
  current.nodePubkey = j["nodePubkey"];
  current.votePubkey = j["votePubkey"];
}

void from_json(const json &j, Delinquent &delinquent) {
  delinquent.activatedStake = j["activatedStake"];
  delinquent.commission = j["commission"];
  delinquent.epochCredits = j["epochCredits"];
  delinquent.epochVoteAccount = j["epochVoteAccount"];
  delinquent.lastVote = j["lastVote"];
  delinquent.nodePubkey = j["nodePubkey"];
  delinquent.votePubkey = j["votePubkey"];
}

void from_json(const json &j, VoteAccounts &voteaccounts) {
  voteaccounts.current = j["current"];
  voteaccounts.delinquent = j["delinquent"];
}

void from_json(const json &j, SignaturesAddress &signaturesaddress) {
  if (!j["blockTime"].is_null()) {
    signaturesaddress.blockTime = std::optional{j["blockTime"]};
  }
  if (!j["err"].is_null()) {
    signaturesaddress.err = std::optional{j["err"]};
  }
  if (!j["confirmationStatus"].is_null()) {
    signaturesaddress.confirmationStatus =
        std::optional{j["confirmationStatus"]};
  }
  if (!j["memo"].is_null()) {
    signaturesaddress.memo = std::optional{j["memo"]};
  }
  signaturesaddress.signature = j["signature"];
  signaturesaddress.slot = j["slot"];
};

void from_json(const json &j, TokenLargestAccounts &tokenlargestaccounts) {
  tokenlargestaccounts.address = j["address"];
  tokenlargestaccounts.amount = j["amount"];
  tokenlargestaccounts.decimals = j["decimals"];
  tokenlargestaccounts.uiAmount = j["uiAmount"];
  tokenlargestaccounts.uiAmountString = j["uiAmountString"];
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

void from_json(const json &j, TokenSupply &tokensupply) {
  tokensupply.amount = j["amount"];
  tokensupply.decimals = j["decimals"];
  tokensupply.uiAmount = j["uiAmount"];
  tokensupply.uiAmountString = j["uiAmountString"];
}

void from_json(const json &j, BlockProduction &blockproduction) {
  blockproduction.firstSlot = j["range"]["firstSlot"];
  blockproduction.lastSlot = j["range"]["lastSlot"];
  auto byIdentity = j["byIdentity"];
  std::vector<std::pair<std::string, std::vector<uint64_t>>> vec;
  for (auto it = byIdentity.begin(); it != byIdentity.end(); ++it) {
    vec.emplace_back(it.key(), it.value());
  }
  blockproduction.byIdentity = vec;
}
void from_json(const json &j, TokenAccountInfo &tokenAccountInfo) {
  tokenAccountInfo.data = j["data"];
  tokenAccountInfo.executable = j["executable"];
  tokenAccountInfo.lamports = j["lamports"];
  tokenAccountInfo.owner = j["owner"];
  tokenAccountInfo.rentEpoch = j["rentEpoch"];
}

void from_json(const json &j, TokenAccountsByOwner &tokenAccountsByOwner) {
  tokenAccountsByOwner.pubkey = j["pubkey"];
  tokenAccountsByOwner.account = j["account"];
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

void to_json(json &j, const GetSupplyConfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.excludeNonCirculatingAccountsList.has_value()) {
    j["excludeNonCirculatingAccountsList"] =
        config.excludeNonCirculatingAccountsList.value();
  }
}

void to_json(json &j, const GetVoteAccountsConfig &config) {
  if (config.commitment.has_value()) {
    j["commitment"] = config.commitment.value();
  }
  if (config.votePubkey.has_value()) {
    j["votePubkey"] = config.votePubkey.value();
  }
  if (config.keepUnstakedDelinquents.has_value()) {
    j["keepUnstakedDelinquents"] = config.keepUnstakedDelinquents.value();
  }
  if (config.delinquentSlotDistance.has_value()) {
    j["keepUnstakedDelinquents"] = config.keepUnstakedDelinquents.value();
  }
}

void to_json(json &j, const GetSignatureAddressConfig &signatureaddressconfig) {
  if (signatureaddressconfig.limit.has_value()) {
    j["limit"] = signatureaddressconfig.limit.value();
  }
  if (signatureaddressconfig.before.has_value()) {
    j["before"] = signatureaddressconfig.before.value();
  }
  if (signatureaddressconfig.until.has_value()) {
    j["until"] = signatureaddressconfig.until.value();
  }
  if (signatureaddressconfig.commitment.has_value()) {
    j["commitment"] = signatureaddressconfig.commitment.value();
  }
  if (signatureaddressconfig.minContextSlot.has_value()) {
    j["minContextSlot"] = signatureaddressconfig.minContextSlot.value();
  }
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
Connection::getLargestAccounts(const LargestAccountsConfig &config) const {
  const json params = {config};
  const auto reqJson = jsonRequest("getLargestAccounts", params);
  const json res = sendJsonRpcRequest(reqJson);
  const std::vector<json> value = res["value"];
  std::vector<LargestAccounts> accounts_list;
  accounts_list.reserve(value.size());
  std::copy(value.begin(), value.end(), std::back_inserter(accounts_list));
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
  std::copy(value.begin(), value.end(), std::back_inserter(samples_list));
  return samples_list;
}

std::vector<std::string> Connection::getSlotLeaders(uint64_t startSlot,
                                                    uint64_t limit) const {
  const json params = {startSlot, limit};
  const json reqJson = jsonRequest("getSlotLeaders", params);
  return sendJsonRpcRequest(reqJson);
}

RpcResponseAndContext<Supply> Connection::getSupply(
    const GetSupplyConfig &config) const {
  const json params = {config};
  const auto reqJson = jsonRequest("getSupply", params);
  const json res = sendJsonRpcRequest(reqJson);
  Supply accounts_list = res["value"];
  return {res["context"], accounts_list};
}

RpcResponseAndContext<TokenAccountBalance> Connection::getTokenAccountBalance(
    const std::string pubkey, const commitmentconfig &config) const {
  const json params = {pubkey, config};
  const auto reqJson = jsonRequest("getTokenAccountBalance", params);
  const json res = sendJsonRpcRequest(reqJson);
  TokenAccountBalance accounts_list = res["value"];
  return {res["context"], accounts_list};
}

VoteAccounts Connection::getVoteAccounts(
    const GetVoteAccountsConfig &config) const {
  const json params = {config};
  const auto reqJson = jsonRequest("getVoteAccounts", params);
  return sendJsonRpcRequest(reqJson);
}

std::vector<SignaturesAddress> Connection::getSignaturesForAddress(
    std::string pubkey,
    const GetSignatureAddressConfig &signatureaddressconfig) const {
  const json params = {pubkey, signatureaddressconfig};
  const auto reqJson = jsonRequest("getSignaturesForAddress", params);
  const json res = sendJsonRpcRequest(reqJson);
  const std::vector<json> value = res;
  std::vector<SignaturesAddress> samples_list;
  samples_list.reserve(value.size());
  std::copy(value.begin(), value.end(), std::back_inserter(samples_list));
  return samples_list;
}

RpcResponseAndContext<std::vector<TokenLargestAccounts>>
Connection::getTokenLargestAccounts(std::string pubkey,
                                    const commitmentconfig &config) const {
  const json params = {pubkey, config};
  const auto reqJson = jsonRequest("getTokenLargestAccounts", params);
  const json res = sendJsonRpcRequest(reqJson);
  const std::vector<json> value = res["value"];
  std::vector<TokenLargestAccounts> accounts_list;
  accounts_list.reserve(value.size());
  std::copy(value.begin(), value.end(), std::back_inserter(accounts_list));
  return {res["context"], accounts_list};
}

std::vector<uint64_t> Connection::getBlocks(
    uint64_t start_slot, uint64_t end_slot,
    const commitmentconfig &config) const {
  const json params = {start_slot, end_slot, config};
  const json reqJson = jsonRequest("getBlocks", params);
  return sendJsonRpcRequest(reqJson);
}

TokenSupply Connection::getTokenSupply(std::string pubkey) const {
  const json params = {pubkey};
  const auto res = jsonRequest("getTokenSupply", params);
  return sendJsonRpcRequest(res)["value"];
}

BlockProduction Connection::getBlockProduction(
    const BlockProductionConfig &config) const {
  const json params = {config};
  const auto res = jsonRequest("getBlockProduction", params);
  return sendJsonRpcRequest(res)["value"];
}

std::vector<std::pair<std::string, std::vector<uint64_t>>>
Connection::getLeaderSchedule(const std::optional<uint64_t> slot,
                              const GetSlotConfig &config) const {
  uint64_t slotArg;
  json params;
  if (slot.has_value()) {
    slotArg = slot.value();
    params = {slotArg, config};
  } else {
    params = {config};
  }
  const auto reqJson = jsonRequest("getLeaderSchedule", params);
  std::vector<std::pair<std::string, std::vector<uint64_t>>> vec;
  const json byIdentity = sendJsonRpcRequest(reqJson);
  for (auto it = byIdentity.begin(); it != byIdentity.end(); ++it) {
    vec.emplace_back(it.key(), it.value());
  }
  return vec;
}

RpcResponseAndContext<std::vector<TokenAccountsByOwner>>
Connection::getTokenAccountsByOwner(
    std::string pubkey, const mintOrProgramIdConfig &mPconfig,
    const TokenAccountsByOwnerConfig &config) const {
  const json params = {pubkey, mPconfig, config};
  const auto reqJson = jsonRequest("getTokenAccountsByOwner", params);
  const json res = sendJsonRpcRequest(reqJson);
  const json value = res["value"];
  return {res["context"], value};
}

namespace subscription {
/**
 * Subscribe to an account to receive notifications when the lamports or data
 * for a given account public key changes
 */
json accountSubscribeRequest(const std::string &account,
                             const Commitment commitment,
                             const std::string &encoding) {
  const json params = {account,
                       {{"commitment", commitment}, {"encoding", encoding}}};

  return rpc::jsonRequest("accountSubscribe", params);
}

WebSocketSubscriber::WebSocketSubscriber(const std::string &host,
                                         const std::string &port,
                                         int timeout_in_seconds) {
  std::promise<void> handshake_promise;
  std::future<void> hanshake_future = handshake_promise.get_future();
  // create a new session
  sess = std::make_shared<session>(
      ioc, timeout_in_seconds,
      std::make_unique<std::promise<void>>(std::move(handshake_promise)));
  std::cout << host << ":" << port << std::endl;
  // function to read
  auto read_fn = [=]() {
    sess->run(host, port);
    ioc.run();
  };

  // writing using one thread and read using another
  read_thread = std::thread(read_fn);
  if (hanshake_future.wait_for(std::chrono::seconds(timeout_in_seconds)) ==
      std::future_status::timeout) {
    std::cerr << "Timeout waiting for subscription request on " << host << ":"
              << port << std::endl;
  }
}
WebSocketSubscriber::~WebSocketSubscriber() {
  // disconnect the session and wait for the threads to complete
  sess->disconnect();
  if (read_thread.joinable()) read_thread.join();
}

/// @brief callback to call when data in account changes
/// @param pub_key public key for the account
/// @param account_change_callback callback to call when the data changes
/// @param commitment commitment
/// @return subsccription id (actually the current id)
int WebSocketSubscriber::onAccountChange(const solana::PublicKey &pub_key,
                                         Callback account_change_callback,
                                         const Commitment &commitment,
                                         Callback on_subscibe,
                                         Callback on_unsubscribe) {
  // create parameters using the user provided input
  json param = {pub_key, {{"encoding", "base64"}, {"commitment", commitment}}};

  // create a new request content
  RequestContent *req = new RequestContent(
      curr_id, "accountSubscribe", "accountUnsubscribe",
      account_change_callback, std::move(param), on_subscibe, on_unsubscribe);

  // subscribe the new request content
  sess->subscribe(req);

  // increase the curr_id so that it can be used for the next request content
  curr_id += 2;

  return req->id;
}

/// @brief remove the account change listener for the given id
/// @param sub_id the id for which removing subscription is needed
void WebSocketSubscriber::removeAccountChangeListener(RequestIdType sub_id) {
  sess->unsubscribe(sub_id);
}
}  // namespace subscription
}  // namespace rpc
}  // namespace solana