#pragma once

#include <cpr/cpr.h>
#include <sodium.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
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

namespace solana {
using json = nlohmann::json;

const std::string NATIVE_MINT = "So11111111111111111111111111111111111111112";
const std::string MEMO_PROGRAM_ID =
    "MemoSq4gqABAXKb96qnH8TysNcWxMyWCqXgDLGmfcHr";
const std::string MAINNET_BETA = "https://api.mainnet-beta.solana.com";
const std::string DEVNET = "https://api.devnet.solana.com";
const int MAXIMUM_NUMBER_OF_BLOCKS_FOR_TRANSACTION = 152;

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
enum Commitment {
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

NLOHMANN_JSON_SERIALIZE_ENUM(Commitment, {
                                             {PROCESSED, "processed"},
                                             {CONFIRMED, "confirmed"},
                                             {FINALIZED, "finalized"},
                                         })

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

/**
 * SignatureStatus to json
 */
void to_json(json &j, const SignatureStatus &status);

/**
 * SignatureStatus from json
 */
void from_json(const json &j, SignatureStatus &status);

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
  /**
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
   * Returns the slot of the lowest confirmed block that has not been purged
   * from the ledger
   **/
  uint64_t getFirstAvailableBlock() const;

  /**
   * Fetch the current status of a signature
   */
  RpcResponseAndContext<std::optional<SignatureStatus>> getSignatureStatus(
      const std::string &signature,
      bool searchTransactionHistory = false) const;

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
inline json accountSubscribeRequest(const std::string &account,
                                    const std::string &commitment = "finalized",
                                    const std::string &encoding = "base64") {
  const json params = {account,
                       {{"commitment", commitment}, {"encoding", encoding}}};

  return rpc::jsonRequest("accountSubscribe", params);
}

class WebSocketSubscriber {
 public:
  net::io_context ioc;
  std::shared_ptr<session> sess;
  std::thread read_thread;
  std::thread write_thread;
  int curr_id = 0;
  std::vector<std::string> available_commitment;

  WebSocketSubscriber(std::string host = "api.devnet.solana.com",
                      std::string port = "80") {
    // create a new session
    sess = std::make_shared<session>(ioc);
    // function to read
    auto read_fn = [=]() {
      // std::cout << host << " " << port << std::endl;
      sess->run(host, port);
      ioc.run();
    };

    // function to write
    auto write_fn = [&]() {
      sess->write();
      ioc.run();
    };

    // add commitments to available commitment
    fill_available_commitment();

    // writing using one thread and read using another
    read_thread = std::thread(read_fn);
    write_thread = std::thread(write_fn);
  }
  ~WebSocketSubscriber() {
    // disconnect the session and wait for the threads to complete
    sess->disconnect();
    if (read_thread.joinable()) read_thread.join();
    if (write_thread.joinable()) write_thread.join();
  }

  /// @brief fill all the available commitments
  void fill_available_commitment() {
    available_commitment.push_back("processed");
    available_commitment.push_back("confirmed");
    available_commitment.push_back("finalized");
  }

  /// @brief check if commitment is part of available commitment
  /// @param commitment the commitment you want to check
  /// @return true or false
  bool check_commitment_integrity(std::string commitment) {
    for (auto com : available_commitment) {
      if (com == commitment) return true;
    }
    return false;
  }
  /// @brief callback to call when data in account changes
  /// @param pub_key public key for the account
  /// @param account_change_callback callback to call when the data changes
  /// @param commitment commitment
  /// @return subsccription id (actually the current id)
  int onAccountChange(std::string pub_key, Callback account_change_callback,
                      std::string commitment = "finalized") {
    // return -1 if commitment is not part of available commitment
    if (!check_commitment_integrity(commitment)) return -1;

    // create parameters using the user provided input
    json param = {pub_key,
                  {{"encoding", "base64"}, {"commitment", commitment}}};

    // create a new request content
    std::shared_ptr<RequestContent> req(
        new RequestContent(curr_id, "accountSubscribe", "accountUnsubscribe",
                           account_change_callback, param));

    // subscribe the new request content
    sess->subscribe(req);

    // increase the curr_id so that it can be used for the next request content
    curr_id += 2;

    return req->id_;
  }

  /// @brief remove the account change listener for the given id
  /// @param sub_id the id for which removing subscription is needed
  void removeAccountChangeListener(int sub_id) { sess->unsubscribe(sub_id); }
};

}  // namespace subscription
}  // namespace rpc
}  // namespace solana
