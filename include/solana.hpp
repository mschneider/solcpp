#pragma once

#include <cpr/cpr.h>
#include <sodium.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "base58.hpp"
#include "base64.hpp"
#include "websocket.hpp"

namespace net = boost::asio;  // from <boost/asio.hpp>

namespace solana {
using json = nlohmann::json;

const std::string NATIVE_MINT = "So11111111111111111111111111111111111111112";
const std::string MEMO_PROGRAM_ID =
    "MemoSq4gqABAXKb96qnH8TysNcWxMyWCqXgDLGmfcHr";
const std::string MAINNET_BETA = "https://api.mainnet-beta.solana.com";
const std::string DEVNET = "https://api.devnet.solana.com";
const int MAXIMUM_NUMBER_OF_BLOCKS_FOR_TRANSACTION = 152;

const int MINIMUM_SLOT_PER_EPOCH = 32;

struct PublicKey {
  static const auto SIZE = crypto_sign_PUBLICKEYBYTES;
  typedef std::array<uint8_t, SIZE> array_t;

  array_t data;

  static PublicKey empty();

  static PublicKey fromBase58(const std::string &b58);

  bool operator==(const PublicKey &other) const;

  std::string toBase58() const;
};

/**
 * PublicKey to json
 */
void to_json(json &j, const PublicKey &key);

/**
 * PublicKey from json
 */
void from_json(const json &j, PublicKey &key);

struct PrivateKey {
  static const size_t SIZE = crypto_sign_SECRETKEYBYTES;
  typedef std::array<uint8_t, SIZE> array_t;

  array_t data;

  std::vector<uint8_t> signMessage(const std::vector<uint8_t> message) const;
};

struct Keypair {
  PublicKey publicKey;
  PrivateKey privateKey;

  static Keypair fromFile(const std::string &path);
};

struct Version {
  uint64_t feature_set;
  std::string solana_core;
};

/**
 * Version from json
 */
void from_json(const json &j, Version &version);

uint64_t trailingZeros(uint64_t n);
uint64_t nextPowerOfTwo(uint64_t n);

struct EpochSchedule {
  uint64_t firstNormalEpoch;
  uint64_t firstNormalSlot;
  uint64_t leaderScheduleSlotOffset;
  uint64_t slotsPerEpoch;
  bool warmup;

  uint64_t getEpoch(uint64_t slot) const {
    return this->getEpochAndSlotIndex(slot)[0];
  }

  std::vector<uint64_t> getEpochAndSlotIndex(uint64_t slot) const {
    std::vector<uint64_t> info;

    if (slot < this->firstNormalSlot) {
      const auto epoch =
          trailingZeros(nextPowerOfTwo(slot + MINIMUM_SLOT_PER_EPOCH + 1)) -
          trailingZeros(MINIMUM_SLOT_PER_EPOCH) - 1;
      const auto epochLen = this->getSlotsInEpoch(epoch);
      const auto slotIndex = slot - (epochLen - MINIMUM_SLOT_PER_EPOCH);
      info.push_back(epoch);
      info.push_back(slotIndex);

      return info;

    } else {
      const auto normalSlotIndex = slot - this->firstNormalSlot;
      const auto normalEpochIndex =
          floor(normalSlotIndex / this->slotsPerEpoch);
      const auto epoch = this->firstNormalEpoch + normalEpochIndex;
      const auto slotIndex = normalSlotIndex % this->slotsPerEpoch;
      info.push_back(epoch);
      info.push_back(slotIndex);

      return info;
    }
  }

  uint64_t getFirstSlotInEpoch(uint64_t epoch) const {
    if (epoch <= this->firstNormalEpoch) {
      return ((1 << epoch) - 1) * MINIMUM_SLOT_PER_EPOCH;
    } else {
      return ((epoch - this->firstNormalEpoch) * this->slotsPerEpoch +
              this->firstNormalSlot);
    }
  }

  uint64_t getLastSlotInEpoch(uint64_t epoch) const {
    return this->getFirstSlotInEpoch(epoch) + this->getSlotsInEpoch(epoch) - 1;
  }

  uint64_t getSlotsInEpoch(uint64_t epoch) const {
    if (epoch < this->firstNormalEpoch) {
      return 1 << (epoch + trailingZeros(MINIMUM_SLOT_PER_EPOCH));
    } else {
      return this->slotsPerEpoch;
    }
  }
};

struct StakeActivation {
  uint64_t active;
  uint64_t inactive;
  std::string state;
};

void from_json(const json &j, StakeActivation &stakeactivation);

struct InflationGovernor {
  double foundation;
  double foundationTerm;
  double initial;
  double taper;
  double terminal;
};

void from_json(const json &j, InflationGovernor &inflationgovernor);
/**
 * EpochSchedule from json
 */
void from_json(const json &j, EpochSchedule &epochschedule);

/**
 * Account metadata used to define instructions
 */
struct AccountMeta {
  PublicKey pubkey;
  bool isSigner;
  bool isWritable;

  bool operator<(const AccountMeta &other) const;
};

struct Instruction {
  PublicKey programId;
  std::vector<AccountMeta> accounts;
  std::vector<uint8_t> data;
};

namespace CompactU16 {
void encode(uint16_t num, std::vector<uint8_t> &buffer);

void encode(const std::vector<uint8_t> &vec, std::vector<uint8_t> &buffer);
};  // namespace CompactU16

struct Blockhash {
  PublicKey publicKey;
  uint64_t lastValidBlockHeight;
};

/**
 * the most-recent return data generated by an instruction in the transaction
 */
struct TransactionReturnData {
  /**
   * the program that generated the return data, as base-58 encoded Pubkey
   */
  std::string programId;
  /**
   * the return data itself, as base-64 encoded binary data
   */
  std::string data;
};

/**
 * TransactionReturnData from json
 */
void from_json(const json &j, TransactionReturnData &data);

/**
 * The level of commitment desired when querying state
 */
enum class Commitment : short {
  /**
   * Query the most recent block which has reached 1 confirmation by the
   * connected node
   */
  PROCESSED,
  /**
   * Query the most recent block which has reached 1 confirmation by the cluster
   */
  CONFIRMED,
  /**
   * Query the most recent block which has been finalized by the cluster
   */
  FINALIZED,
};

NLOHMANN_JSON_SERIALIZE_ENUM(Commitment,
                             {
                                 {Commitment::PROCESSED, "processed"},
                                 {Commitment::CONFIRMED, "confirmed"},
                                 {Commitment::FINALIZED, "finalized"},
                             })

struct GetStakeActivationConfig {
  /** The level of commitment desired */
  std::optional<Commitment> commitment = std::nullopt;
  std::optional<uint64_t> epoch = std::nullopt;
  /** The minimum slot that the request can be evaluated at */
  std::optional<uint64_t> minContextSlot = std::nullopt;
};

void to_json(json &j, const GetStakeActivationConfig &config);

struct commitmentconfig {
  /** The level of commitment desired */
  std::optional<Commitment> commitment = std::nullopt;
};
void to_json(json &j, const commitmentconfig &config);

struct GetBlocksConfig {
  std::optional<uint64_t> end_slot = std::nullopt;
  std::optional<Commitment> commitment = std::nullopt;
};
void to_json(json &j, const GetBlocksConfig &config);

struct SimulatedTransactionResponse {
  /**
   * Error if transaction failed, null if transaction succeeded.
   */
  std::optional<std::string> err = std::nullopt;
  /**
   * array of accounts with the same length as the accounts.addresses array in
   * the request
   */
  std::optional<std::vector<std::string>> accounts = std::nullopt;
  /**
   * Array of log messages the transaction instructions output during execution,
   * null if simulation failed before the transaction was able to execute (for
   * example due to an invalid blockhash or signature verification failure)
   */
  std::optional<std::vector<std::string>> logs = std::nullopt;
  /**
   * The number of compute budget units consumed during the processing of this
   * transaction
   */
  std::optional<uint64_t> unitsConsumed = std::nullopt;
  /**
   * the most-recent return data generated by an instruction in the transaction
   */
  std::optional<TransactionReturnData> returnData = std::nullopt;
};

/**
 * SimulatedTransactionResponse from json
 */
void from_json(const json &j, SimulatedTransactionResponse &res);

struct GetSlotConfig {
  /** The level of commitment desired */
  std::optional<Commitment> commitment = std::nullopt;
  /** The minimum slot that the request can be evaluated at */
  std::optional<uint64_t> minContextSlot = std::nullopt;
};

/**
 * convert GetSlotConfig to json
 */
void to_json(json &j, const GetSlotConfig &config);

struct LargestAccountsConfig {
  std::optional<Commitment> commitment = std::nullopt;
  std::optional<std::string> filter = std::nullopt;
};
void to_json(json &j, const LargestAccountsConfig &config);

struct SignatureStatus {
  /** when the transaction was processed */
  uint64_t slot;
  /** the number of blocks that have been confirmed and voted on in the fork
   * containing `slot` */
  std::optional<uint64_t> confirmations = std::nullopt;
  /** transaction error, if any */
  std::optional<std::string> err = std::nullopt;
  /** cluster confirmation status, if data available. Possible responses:
   * `processed`, `confirmed`, `finalized` */
  Commitment confirmationStatus;
};

struct EpochInfo {
  uint64_t absoluteSlot;
  uint64_t blockHeight;
  uint64_t epoch;
  uint64_t slotIndex;
  uint64_t slotsInEpoch;
  uint64_t transactionCount;
};

void from_json(const json &j, EpochInfo &epochinfo);

struct Nodes {
  std::optional<uint64_t> featureSet = std::nullopt;
  std::optional<std::string> gossip = std::nullopt;
  std::optional<std::string> pubkey = std::nullopt;
  std::optional<std::string> rpc = std::nullopt;
  std::optional<uint64_t> shredVersion = std::nullopt;
  std::optional<std::string> tpu = std::nullopt;
  std::optional<std::string> version = std::nullopt;
};

void from_json(const json &j, Nodes &nodes);

struct LargestAccounts {
  uint64_t lamports;
  std::string address;
};

void from_json(const json &j, LargestAccounts &largestaccounts);

struct RecentPerformanceSamples {
  uint64_t numSlots;
  uint64_t numTransactions;
  uint64_t samplePeriodSecs;
  uint64_t slot;
};
void from_json(const json &j,
               RecentPerformanceSamples &recentperformancesamples);

struct getFeeForMessageRes {
  std::optional<uint64_t> value = std::nullopt;
};
void from_json(const json &j, getFeeForMessageRes &res);

/**
 * SignatureStatus to json
 */
void to_json(json &j, const SignatureStatus &status);

/**
 * SignatureStatus from json
 */
void from_json(const json &j, SignatureStatus &status);

struct GetSupplyConfig {
  /** The level of commitment desired */
  std::optional<Commitment> commitment = std::nullopt;
  /** The minimum slot that the request can be evaluated at */
  std::optional<bool> excludeNonCirculatingAccountsList = std::nullopt;
};

struct GetVoteAccountsConfig {
  std::optional<Commitment> commitment = std::nullopt;
  std::optional<std::string> votePubkey = std::nullopt;
  std::optional<bool> keepUnstakedDelinquents = std::nullopt;
  std::optional<uint64_t> delinquentSlotDistance = std::nullopt;
};

struct GetSignatureAddressConfig {
  std::optional<uint64_t> limit = std::nullopt;
  std::optional<std::string> before = std::nullopt;
  std::optional<std::string> until = std::nullopt;
  std::optional<Commitment> commitment = std::nullopt;
  std::optional<uint64_t> minContextSlot = std::nullopt;
};

struct Supply {
  uint64_t circulating;
  uint64_t nonCirculating;
  std::optional<std::vector<std::string>> nonCirculatingAccounts = std::nullopt;
  uint64_t total;
};

void from_json(const json &j, Supply &supply);

struct TokenAccountBalance {
  std::string amount;
  uint64_t decimals;
  double uiAmount;
  std::string uiAmountString;
};

void from_json(const json &j, TokenAccountBalance &tokenaccountbalance);

struct Current {
  uint64_t commission;
  bool epochVoteAccount;
  std::vector<std::vector<uint64_t>> epochCredits;
  std::string nodePubkey;
  uint64_t lastVote;
  uint64_t activatedStake;
  std::string votePubkey;
};

struct Delinquent {
  uint64_t commission;
  bool epochVoteAccount;
  std::vector<std::vector<uint64_t>> epochCredits;
  std::string nodePubkey;
  uint64_t lastVote;
  uint64_t activatedStake;
  std::string votePubkey;
};

struct VoteAccounts {
  std::vector<Current> current;
  std::vector<Delinquent> delinquent;
};

struct SignaturesAddress {
  std::optional<uint64_t> blockTime = std::nullopt;
  std::optional<Commitment> confirmationStatus = std::nullopt;
  std::optional<std::string> err = std::nullopt;
  std::optional<std::string> memo = std::nullopt;
  std::string signature;
  uint64_t slot;
};

void from_json(const json &j, SignaturesAddress &signaturesaddress);
void to_json(json &j, const GetSupplyConfig &config);
void to_json(json &j, const GetVoteAccountsConfig &config);

/**
 * Extra contextual information for RPC responses
 */
struct Context {
  uint64_t slot;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Context, slot);

/**
 * RPC Response with extra contextual information
 */
template <typename T>
struct RpcResponseAndContext {
  /** response context */
  Context context;
  /** response value */
  T value;
};

/**
 * Information describing an account
 */
template <typename T>
struct AccountInfo {
  /**
   * boolean indicating if the account contains a program (and is strictly
   * read-only)
   */
  bool executable;
  /**
   * base-58 encoded Pubkey of the program this account has been assigned to
   */
  PublicKey owner;
  /**namespace net = boost::asio; // from <boost/asio.hpp>
   * number of lamports assigned to this account
   */
  uint64_t lamports;
  /**
   * data associated with the account as encoded binary
   */
  T data;
  /**
   * the epoch at which this account will next owe rent
   */
  uint64_t rentEpoch;
};

/**
 * AccountInfo from json
 */
template <typename T>
void from_json(const json &j, AccountInfo<T> &info) {
  info.executable = j["executable"];
  info.owner = PublicKey::fromBase58(j["owner"]);
  info.lamports = j["lamports"];
  // check
  assert(j["data"][1] == BASE64);
  // b64 decode data for casting
  const auto decoded_data = b64decode(j["data"][0]);
  // decoded data should fit into T
  if (decoded_data.size() != sizeof(T))
    throw std::runtime_error("invalid response length " +
                             std::to_string(decoded_data.size()) +
                             " expected " + std::to_string(sizeof(T)));

  // cast
  info.data = T{};
  memcpy(&info.data, decoded_data.data(), sizeof(T));

  info.rentEpoch = j["rentEpoch"];
}

/**
 * AccountInfo from json
 */
template <typename T>
void from_json(const json &j, std::optional<AccountInfo<T>> &info) {
  if (j.is_null()) {
    info = std::nullopt;
  } else {
    info = std::optional<AccountInfo<T>>{j};
  }
}

/**
 * An instruction to execute by a program
 */
struct CompiledInstruction {
  uint8_t programIdIndex;
  std::vector<uint8_t> accountIndices;
  std::vector<uint8_t> data;

  static CompiledInstruction fromInstruction(
      const Instruction &ix, const std::vector<PublicKey> &accounts);

  void serializeTo(std::vector<uint8_t> &buffer) const;
};

struct CompiledTransaction {
  Blockhash recentBlockhash;
  std::vector<PublicKey> accounts;
  std::vector<CompiledInstruction> instructions;
  uint8_t requiredSignatures;
  uint8_t readOnlySignedAccounts;
  uint8_t readOnlyUnsignedAccounts;

  static CompiledTransaction fromInstructions(
      const std::vector<Instruction> &instructions, const PublicKey &payer,
      const Blockhash &blockhash);

  void serializeTo(std::vector<uint8_t> &buffer) const;

  /**
   * sign the transaction
   */
  static std::vector<uint8_t> signTransaction(const Keypair &keypair,
                                              const std::vector<uint8_t> &tx);

  /**
   * sign the CompiledTransaction
   */
  std::vector<uint8_t> sign(const Keypair &keypair) const;
};

namespace rpc {
const std::string JSON_PARSED = "jsonParsed";

using json = nlohmann::json;

json jsonRequest(const std::string &method, const json &params = nullptr);

/**
 * Read AccountInfo dumped in a file
 * @param path Path to file
 */
template <typename T>
static T fromFile(const std::string &path) {
  std::ifstream fileStream(path);
  std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
  auto response = json::parse(fileContent);
  const std::string encoded = response["data"][0];
  const std::string decoded = solana::b64decode(encoded);
  if (decoded.size() != sizeof(T))
    throw std::runtime_error("Invalid account data");
  T accountInfo{};
  memcpy(&accountInfo, decoded.data(), sizeof(T));
  return accountInfo;
}

/**
 * Configuration object for sendTransaction
 */
struct SendTransactionConfig {
  /**
   * if true, skip the preflight transaction checks (default: false)
   */
  const std::optional<bool> skipPreflight = std::nullopt;
  /**
   * Commitment level to use for preflight (default: "finalized").
   */
  const std::optional<Commitment> preflightCommitment = std::nullopt;
  /**
   * Encoding used for the transaction data. Either "base58" (slow, DEPRECATED),
   * or "base64". (default: "base64", rpc default: "base58").
   */
  const std::string encoding = BASE64;
  /**
   * Maximum number of times for the RPC node to retry sending the transaction
   * to the leader. If this parameter not provided, the RPC node will retry the
   * transaction until it is finalized or until the blockhash expires.
   */
  const std::optional<uint8_t> maxRetries = std::nullopt;
  /**
   * set the minimum slot at which to perform preflight transaction checks.
   */
  const std::optional<uint8_t> minContextSlot = std::nullopt;
};

/**
 * SendTransactionConfig to json
 */
void to_json(json &j, const SendTransactionConfig &config);

///
/// Configuration object for simulateTransaction
struct SimulateTransactionConfig {
  /**
   * if true the transaction signatures will be verified (default: false,
   * conflicts with replaceRecentBlockhash)
   */
  const std::optional<bool> sigVerify = std::nullopt;
  /**
   * Commitment level to simulate the transaction at (default: "finalized").
   */
  const std::optional<Commitment> commitment = std::nullopt;
  /**
   *if true the transaction recent blockhash will be replaced with the most
   *recent blockhash. (default: false, conflicts with sigVerify)
   */
  const std::optional<bool> replaceRecentBlockhash = std::nullopt;
  /**
   * An array of accounts to return, as base-58 encoded strings
   */
  const std::optional<std::vector<std::string>> address = std::nullopt;
  /**
   * set the minimum slot that the request can be evaluated at.
   */
  const std::optional<uint8_t> minContextSlot = std::nullopt;
};

/**
 * convert SimulateTransactionConfig to json for RPC request param
 */
void to_json(json &j, const SimulateTransactionConfig &config);

/**
 * Data slice argument to limit the returned account data
 */
struct DataSlice {
  /** offset of data slice */
  uint16_t offset;
  /** length of data slice */
  uint16_t number;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DataSlice, offset, number);

/**
 * Configuration object for changing `getAccountInfo` and
 * `getMultipleAccountsInfo` query behavior
 */
struct GetAccountInfoConfig {
  /**
   * The level of commitment desired
   */
  std::optional<std::string> commitment = std::nullopt;
  /**
   * The minimum slot that the request can be evaluated at
   */
  std::optional<uint64_t> minContextSlot = std::nullopt;
  /**
   * Optional data slice to limit the returned account data
   */
  std::optional<DataSlice> dataSlice = std::nullopt;
};

/**
 * convert GetAccountInfoConfig to json
 */
void to_json(json &j, const GetAccountInfoConfig &config);

///
/// RPC HTTP Endpoints
class Connection {
 public:
  /**
   * Initialize the rpc url and commitment levels to use.
   * Initialize sodium
   */
  Connection(const std::string &rpc_url = MAINNET_BETA);
  /*
   * send rpc request
   * @return result from response
   */
  json sendJsonRpcRequest(const json &body) const;

  /**
   * @deprecated
   * Sign and send a transaction
   * @return transaction signature
   */
  [[deprecated]] std::string signAndSendTransaction(
      const Keypair &keypair, const CompiledTransaction &tx,
      bool skipPreflight = false,
      const Commitment &preflightCommitment = Commitment::FINALIZED) const;

  /**
   * Sign and send a transaction
   * @return transaction signature
   */
  std::string sendTransaction(
      const Keypair &keypair, const CompiledTransaction &tx,
      const SendTransactionConfig &config = SendTransactionConfig()) const;

  /**
   * Send a transaction that has already been signed and serialized into the
   * wire format
   */
  std::string sendRawTransaction(
      const std::vector<uint8_t> &tx,
      const SendTransactionConfig &config = SendTransactionConfig()) const;

  /**
   * Send a transaction that has already been signed, serialized into the
   * wire format, and encoded as a base64 string
   */
  std::string sendEncodedTransaction(
      const std::string &transaction,
      const SendTransactionConfig &config = SendTransactionConfig()) const;

  /**
   * Simulate sending a transaction
   * @return SimulatedTransactionResponse
   */
  SimulatedTransactionResponse simulateTransaction(
      const Keypair &keypair, const CompiledTransaction &tx,
      const SimulateTransactionConfig &config =
          SimulateTransactionConfig()) const;

  /**
   * Request an allocation of lamports to the specified address
   */
  std::string requestAirdrop(const PublicKey &pubkey, uint64_t lamports) const;

  /**
   * Fetch the balance for the specified public key
   */
  uint64_t getBalance(const PublicKey &pubkey) const;

  /**
   * Fetch a recent blockhash from the cluster
   * @deprecated Deprecated since Solana v1.8.0. Please use {@link
   * getLatestBlockhash} instead.
   * @return Blockhash
   */
  [[deprecated]] PublicKey getRecentBlockhash(
      const Commitment &commitment = Commitment::FINALIZED) const;

  /**
   * Fetch the latest blockhash from the cluster
   */
  Blockhash getLatestBlockhash(
      const Commitment &commitment = Commitment::FINALIZED) const;

  /**
   * Returns the current block height of the node
   */
  uint64_t getBlockHeight(
      const Commitment &commitment = Commitment::FINALIZED) const;

  /**
   * Returns of the current Transaction has been confirmed or not
   */
  bool confirmTransaction(std::string transactionSignature,
                          Commitment confirmLevel,
                          uint16_t timeout = 200) const;

  /**
   * Fetch the current statuses of a batch of signatures
   */
  RpcResponseAndContext<std::vector<std::optional<SignatureStatus>>>
  getSignatureStatuses(const std::vector<std::string> &signatures,
                       bool searchTransactionHistory = false) const;

  /**
   * Returns the current solana versions running on the node
   **/
  Version getVersion() const;

  /**
   *  Returns the lowest slot that the node has information about in its ledger.
   **/
  uint64_t minimumLedgerSlot() const;

  /**
   *  Returns the genesis hash
   **/
  std::string getGenesisHash() const;

  /**
   * Returns epoch schedule information from this cluster's genesis config
   **/
  EpochSchedule getEpochSchedule() const;

  /**
   * Returns the slot that has reached the given or default commitment level
   **/
  uint64_t getSlot(const GetSlotConfig &config = GetSlotConfig{}) const;

  /**
   * Returns the current slot leader
   **/
  std::string getSlotLeader(
      const GetSlotConfig &config = GetSlotConfig{}) const;

  /**
   * Returns the slot of the lowest confirmed block that has not been purged
   *from the ledger
   **/
  uint64_t getFirstAvailableBlock() const;

  /**
   * Returns epoch activation information for a stake account
   **/
  StakeActivation getStakeActivation(const PublicKey &pubkey,
                                     const GetStakeActivationConfig &config =
                                         GetStakeActivationConfig{}) const;

  /**
   * Returns the current inflation governor
   **/
  InflationGovernor getInflationGovernor(
      const commitmentconfig &config = commitmentconfig{}) const;

  /**
   * Returns the current Transaction count from the ledger
   **/
  uint64_t getTransactionCount(
      const GetSlotConfig &config = GetSlotConfig{}) const;

  /**
   * Returns information about the current epoch
   **/
  EpochInfo getEpochInfo(const GetSlotConfig &config = GetSlotConfig{}) const;

  /**
   * Returns minimum balance required to make account rent exempt
   **/
  uint64_t getMinimumBalanceForRentExemption(
      const std::size_t dataLength,
      const commitmentconfig &config = commitmentconfig{}) const;

  /**
   * Returns the estimated production time of a block.
   */
  uint64_t getBlockTime(const uint64_t slot) const;

  /**
   *Returns information about all the nodes participating in the cluster
   */
  std::vector<Nodes> getClusterNodes() const;

  /**RequestIdType
   *Get the fee the network will charge for a particular Message
   */
  getFeeForMessageRes getFeeForMessage(
      const std::string message,
      const GetSlotConfig &config = GetSlotConfig{}) const;

  /**
   *Returns a list of recent performance samples, in reverse slot order.
   */
  std::vector<RecentPerformanceSamples> getRecentPerformanceSamples(
      std::size_t limit) const;

  /**
   *Returns the 20 largest accounts, by lamport balance
   */
  RpcResponseAndContext<std::vector<LargestAccounts>> getLargestAccounts(
      const LargestAccountsConfig &config = LargestAccountsConfig{}) const;

  /**
   * Fetch the current status of a signature
   */
  RpcResponseAndContext<std::optional<SignatureStatus>> getSignatureStatus(
      const std::string &signature,
      bool searchTransactionHistory = false) const;

  /**
   * Returns the slot leaders for a given slot range
   */
  std::vector<std::string> getSlotLeaders(uint64_t startSlot,
                                          uint64_t limit) const;

  /**
   * Returns information about the current supply.
   */
  RpcResponseAndContext<Supply> getSupply(
      const GetSupplyConfig &config = GetSupplyConfig{}) const;

  /**
   * Returns the token balance of an SPL Token account.
   */
  RpcResponseAndContext<TokenAccountBalance> getTokenAccountBalance(
      const std::string pubkey,
      const commitmentconfig &config = commitmentconfig{}) const;

  /**
   * Returns the account info and associated stake for all the voting accounts
   * in the current bank.
   */
  VoteAccounts getVoteAccounts(
      const GetVoteAccountsConfig &config = GetVoteAccountsConfig{}) const;

  /**
   * Returns signatures for confirmed transactions that include the given
   * address in their accountKeys list.
   */
  std::vector<SignaturesAddress> getSignaturesForAddress(
      std::string pubkey, const GetSignatureAddressConfig &config =
                              GetSignatureAddressConfig{}) const;

  /**
   * Fetch parsed account info for the specified public key
   */
  template <typename T>
  RpcResponseAndContext<std::optional<AccountInfo<T>>> getAccountInfo(
      const PublicKey &publicKey,
      const GetAccountInfoConfig &config = GetAccountInfoConfig{}) const {
    // create request
    const json params = {publicKey, config};
    const json reqJson = jsonRequest("getAccountInfo", params);
    // send jsonRpc request
    const json res = sendJsonRpcRequest(reqJson);
    const json value = res["value"];
    if (value.is_null()) {
      return {res["context"], std::nullopt};
    } else {
      return {res["context"], std::optional<AccountInfo<T>>{value}};
    }
  }

  /**
   * Fetch all the account info for multiple accounts specified by an array of
   * public keys
   */
  template <typename T>
  RpcResponseAndContext<std::vector<std::optional<AccountInfo<T>>>>
  getMultipleAccountsInfo(
      const std::vector<PublicKey> &publicKeys,
      const GetAccountInfoConfig &config = GetAccountInfoConfig{}) const {
    // create request
    const json params = {publicKeys, config};
    const json reqJson = jsonRequest("getMultipleAccounts", params);
    // send jsonRpc request
    const json res = sendJsonRpcRequest(reqJson);

    return {res["context"], res["value"]};
  }

 private:
  const std::string &rpc_url_;
};

///
/// Websocket requests
namespace subscription {

/**
 * Subscribe to an account to receive notifications when the lamports or data
 * for a given account public key changes
 */
json accountSubscribeRequest(
    const std::string &account,
    const Commitment commitment = Commitment::FINALIZED,
    const std::string &encoding = BASE64);

class WebSocketSubscriber {
 public:
  net::io_context ioc;
  std::shared_ptr<session> sess;
  std::thread read_thread;
  RequestIdType curr_id = 0;
  std::vector<std::string> available_commitment;

  WebSocketSubscriber(const std::string &host, const std::string &port,
                      int timeout_in_seconds = 30);
  ~WebSocketSubscriber();

  /// @brief callback to call when data in account changes
  /// @param pub_key public key for the account
  /// @param account_change_callback callback to call when the data changes
  /// @param commitment commitment
  /// @return subsccription id (actually the current id)
  int onAccountChange(const solana::PublicKey &pub_key,
                      Callback account_change_callback,
                      const Commitment &commitment = Commitment::FINALIZED,
                      Callback on_subscibe = nullptr,
                      Callback on_unsubscribe = nullptr);

  /// @brief remove the account change listener for the given id
  /// @param sub_id the id for which removing subscription is needed
  void removeAccountChangeListener(RequestIdType sub_id);
};
}  // namespace subscription
}  // namespace rpc
}  // namespace solana
