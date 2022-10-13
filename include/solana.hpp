#pragma once

#include <cpr/cpr.h>
#include <sodium.h>

#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "base58.hpp"
#include "base64.hpp"

namespace solana {
using json = nlohmann::json;

const std::string NATIVE_MINT = "So11111111111111111111111111111111111111112";
const std::string MEMO_PROGRAM_ID =
    "MemoSq4gqABAXKb96qnH8TysNcWxMyWCqXgDLGmfcHr";
const std::string MAINNET_BETA = "https://api.mainnet-beta.solana.com";
const std::string DEVNET = "https://api.devnet.solana.com";

struct PublicKey {
  static const auto SIZE = crypto_sign_PUBLICKEYBYTES;
  typedef std::array<uint8_t, SIZE> array_t;

  array_t data;

  static PublicKey empty();

  static PublicKey fromBase58(const std::string &b58);

  bool operator==(const PublicKey &other) const;

  std::string toBase58() const;
};

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

struct SimulatedTransactionResponse {
  std::optional<std::string> err = std::nullopt;
  std::optional<std::vector<std::string>> accounts = std::nullopt;
  std::optional<std::vector<std::string>> logs = std::nullopt;
  std::optional<uint64_t> unitsConsumed = std::nullopt;
  // returnData?: TransactionReturnData | null;
};

/**
 * SimulatedTransactionResponse to json
 */
void to_json(json &j, const SimulatedTransactionResponse &res);

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
  std::string confirmationStatus;
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
  const std::optional<std::string> preflightCommitment = std::nullopt;
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

  /**
   * serialize to json
   */
  json toJson() const;
};

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
  const std::optional<std::string> commitment = std::nullopt;
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

  /**
   * convert to json for RPC request param
   */
  json toJson() const;
};

///
/// RPC HTTP Endpoints
class Connection {
 public:
  /**
   * Initialize the rpc url and commitment levels to use.
   * Initialize sodium
   */
  Connection(const std::string &rpc_url = MAINNET_BETA,
             const std::string &commitment = "finalized");
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
      const std::string &preflightCommitment = "finalized") const;

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
      const std::string &commitment = "finalized") const;

  /**
   * Fetch the latest blockhash from the cluster
   */
  Blockhash getLatestBlockhash(
      const std::string &commitment = "finalized") const;

  /**
   * Returns the current block height of the node
   */
  uint64_t getBlockHeight(const std::string &commitment = "finalized") const;

  /**
   * Fetch the current statuses of a batch of signatures
   */
  std::vector<SignatureStatus> getSignatureStatuses(
      const std::vector<std::string> &signatures,
      bool searchTransactionHistory = false) const;

  /**
   * Fetch the current status of a signature
   */
  SignatureStatus getSignatureStatus(
      const std::string &signature,
      bool searchTransactionHistory = false) const;

  /**
   * Fetch all the account info for the specified public key
   */
  template <typename T>
  inline T getAccountInfo(const std::string &account,
                          const std::string &encoding = "base64",
                          const size_t offset = 0,
                          const size_t length = 0) const {
    // create Params
    json params = {account};
    json options = {{"encoding", encoding}};
    if (offset && length) {
      json dataSlice = {"dataSlice", {{"offset", offset}, {"length", length}}};
      options.emplace(dataSlice);
    }
    params.emplace_back(options);
    // create request
    const auto reqJson = jsonRequest("getAccountInfo", params);
    // send jsonRpc request
    const json res = sendJsonRpcRequest(reqJson);
    // Account info from response
    const std::string encoded = res["value"]["data"][0];
    const std::string decoded = b64decode(encoded);
    if (decoded.size() != sizeof(T))  // decoded should fit into T
      throw std::runtime_error("invalid response length " +
                               std::to_string(decoded.size()) + " expected " +
                               std::to_string(sizeof(T)));

    T result{};
    memcpy(&result, decoded.data(), sizeof(T));
    return result;
  }

  /**
   * Returns account information for a list of pubKeys
   * Returns a map of {pubKey : AccountInfo} for accounts that exist.
   * Accounts that don't exist return a `null` result and are skipped
   */
  template <typename T>
  inline std::map<std::string, T> getMultipleAccounts(
      const std::vector<std::string> &accounts,
      const std::string &encoding = "base64", const size_t offset = 0,
      const size_t length = 0) const {
    // create params
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
    // create request
    const json reqJson = jsonRequest("getMultipleAccounts", params);
    // send jsonRpc request
    const json res = sendJsonRpcRequest(reqJson);
    const auto &account_info_vec = res["value"];
    std::map<std::string, T> result;
    int index = -1;
    for (const auto &account_info : account_info_vec) {
      ++index;
      if (account_info.is_null()) continue;  // Account doesn't exist
      const std::string encoded = account_info["data"][0];
      const std::string decoded = b64decode(encoded);
      if (decoded.size() != sizeof(T))
        throw std::runtime_error("invalid response length " +
                                 std::to_string(decoded.size()) + " expected " +
                                 std::to_string(sizeof(T)));
      T account{};
      memcpy(&account, decoded.data(), sizeof(T));
      result[reqJson["params"][0][index]] =
          account;  // Retrieve the corresponding
                    // pubKey from the request
    }
    return result;
  }

 private:
  const std::string &rpc_url_;
  const std::string &commitment_;
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

}  // namespace subscription
}  // namespace rpc
}  // namespace solana
