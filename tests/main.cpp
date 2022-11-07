#include <boost/regex.hpp>
#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>
#include <thread>

#include "solana.hpp"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "MangoAccount.hpp"

const std::string KEY_PAIR_FILE = "../tests/fixtures/solana/id.json";
const std::string DEVNET_GENESIS_HASH =
    "EtWTRABZaYq6iMfeYKouRu166VU2xqa1wcaWoxPkrZBG";

TEST_CASE("Simulate & Send Transaction") {
  const solana::Keypair keyPair = solana::Keypair::fromFile(KEY_PAIR_FILE);
  auto connection = solana::rpc::Connection(solana::DEVNET);

  // create a compiled transaction
  auto recentBlockHash = connection.getLatestBlockhash();
  const solana::PublicKey memoProgram =
      solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
  const std::string memo = "Hello";

  const solana::CompiledInstruction ix = {
      1, {}, std::vector<uint8_t>(memo.begin(), memo.end())};

  const solana::CompiledTransaction compiledTx = {
      recentBlockHash, {keyPair.publicKey, memoProgram}, {ix}, 1, 0, 1};

  ///
  /// Test simulateTransaction
  ///
  const auto simulateRes = connection.simulateTransaction(keyPair, compiledTx);
  // consumed units should be greater than unity
  CHECK_GT(simulateRes.unitsConsumed.value(), 0);
  CHECK_FALSE(simulateRes.logs.value().empty());
  CHECK_FALSE(simulateRes.accounts.has_value());

  ///
  /// Test sendTransaction
  ///
  const std::string transactionSignature =
      connection.sendTransaction(keyPair, compiledTx);
  // serialize transaction
  std::vector<uint8_t> tx;
  compiledTx.serializeTo(tx);
  // create signature
  const auto signedTx = keyPair.privateKey.signMessage(tx);
  // encode the signature
  const auto b58Sig =
      solana::b58encode(std::string(signedTx.begin(), signedTx.end()));

  CHECK_EQ(transactionSignature, b58Sig);
}

TEST_CASE("Request Airdrop & Confirm Transaction") {
  const solana::Keypair keyPair = solana::Keypair::fromFile(KEY_PAIR_FILE);
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  // request Airdrop
  const auto prev_sol = connection.getBalance(keyPair.publicKey);
  const auto signature = connection.requestAirdrop(keyPair.publicKey, 50001);
  uint8_t retries = 140;
  // using confirmTransaction to check if the airdrop went through
  bool confirmed = false;
  confirmed = connection.confirmTransaction(
      signature, solana::Commitment::FINALIZED, retries);
  const auto new_sol = connection.getBalance(keyPair.publicKey);
  CHECK_EQ(confirmed, true);
  CHECK_GT(new_sol, prev_sol);
}

TEST_CASE("base58 decode & encode") {
  const std::vector<std::string> bs58s{
      "98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue",
      "mv3ekLzLbnVPNxjSKvqBpU3ZeZXPQdEC3bp5MDEBG68",
      "9xQeWvG816bUx9EPjHmaT23yvVM2ZWbrrpZb9PusVFin",
      "MangoCzJ36AjZyKwVj3VnYU4GTonjfVEnJmvvWaxLac",
      "14ivtgssEBoBjuZJtSAPKYgpUK7DmnSwuPMqJoVTSgKJ"};

  std::string resources_dir = FIXTURES_DIR;
  for (const auto& bs58 : bs58s) {
    const auto decoded = solana::b58decode(bs58);
    const auto encoded = solana::b58encode(decoded);
    const auto redecoded = solana::b58decode(encoded);
    std::ifstream fixture(resources_dir + "/base58/" + bs58, std::ios::binary);
    std::vector<char> buffer(std::istreambuf_iterator<char>(fixture), {});

    CHECK_EQ(bs58.size(), encoded.size());
    CHECK_EQ(bs58, encoded);
    CHECK_EQ(decoded.size(), redecoded.size());
    CHECK_EQ(decoded, redecoded);
    CHECK_EQ(decoded.size(), buffer.size());
    CHECK_EQ(decoded, std::string(buffer.begin(), buffer.end()));
  }
}

TEST_CASE("parse private keys") {
  std::string resources_dir = FIXTURES_DIR;
  const auto keypair =
      solana::Keypair::fromFile(resources_dir + "/solana/id.json");
  CHECK_EQ("8K4Exjnvs3ZJQDE78zmFoax5Sh4cEVdbk1D1r17Wxuud",
           keypair.publicKey.toBase58());
}

TEST_CASE("decode mango_v3 Fill") {
  const std::string encoded(
      "AAEMAAEAAAB6PABiAAAAAJMvAwAAAAAAEp7AH3xFwgByZdzdjJaK2f9K+"
      "nwfGkKL3EBs6qBSkbT0Wsj+/////3JYBgAAAAAAPNHr0H4BAADkFB3J5f//////////////"
      "AAAAAAAAAABfPABiAAAAABh0e79OvRxWYgRL9dtu02f5VK/SK/"
      "CK1oU+"
      "Tgm1NbL9IaU3AQAAAADOMAYAAAAAAAAAAAAAAAAA46WbxCAAAAAAAAAAAAAAAHJYBgAAAAAA"
      "AQAAAAAAAAA=");
  const std::string decoded = solana::b64decode(encoded);
  const mango_v3::FillEvent* event = (mango_v3::FillEvent*)decoded.data();
  CHECK_EQ(event->eventType, mango_v3::EventType::Fill);
  CHECK_EQ(event->takerSide, mango_v3::Side::Sell);
  CHECK_EQ(event->makerOut, 0);
  CHECK_EQ(event->timestamp, 1644182650);
  CHECK_EQ(event->seqNum, 208787);
  CHECK_EQ(event->maker.toBase58(),
           "2Fgjpc7bp9jpiTRKSVSsiAcexw8Cawbz7GLJu8MamS9q");
  CHECK_EQ(to_string(event->makerOrderId), "7671244543748780405054196");
  CHECK_EQ(event->makerClientOrderId, 1644182622524);
  CHECK_EQ((int)round(event->makerFee.to_double() * 10000), -4);
  CHECK_EQ(event->bestInitial, 0);
  CHECK_EQ(event->makerTimestamp, 1644182623);
  CHECK_EQ(event->taker.toBase58(),
           "2eTob7jrhKeHNhkK1jTfS3kZYdtNQS1VF7LETom6YHjJ");
  CHECK_EQ(to_string(event->takerOrderId), "7484028538144702206551329");
  CHECK_EQ(event->takerClientOrderId, 0);
  CHECK_EQ((int)round(event->takerFee.to_double() * 10000), 5);
  CHECK_EQ(event->price, 415858);
  CHECK_EQ(event->quantity, 1);
}

TEST_CASE("compile memo transaction") {
  const solana::Blockhash recentBlockhash = {};
  const auto feePayer = solana::PublicKey::fromBase58(
      "8K4Exjnvs3ZJQDE78zmFoax5Sh4cEVdbk1D1r17Wxuud");
  const auto memoProgram =
      solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
  const std::string memo = "Hello \xF0\x9F\xA5\xAD";
  const solana::Instruction ix = {
      memoProgram, {}, std::vector<uint8_t>(memo.begin(), memo.end())};
  const auto ctx = solana::CompiledTransaction::fromInstructions(
      {ix}, feePayer, recentBlockhash);

  CHECK_EQ(recentBlockhash.publicKey, ctx.recentBlockhash.publicKey);
  CHECK_EQ(2, ctx.accounts.size());
  CHECK_EQ(feePayer, ctx.accounts[0]);
  CHECK_EQ(memoProgram, ctx.accounts[1]);
  CHECK_EQ(1, ctx.instructions[0].programIdIndex);
  CHECK_EQ(0, ctx.instructions[0].accountIndices.size());
  CHECK_EQ(ix.data, ctx.instructions[0].data);
  CHECK_EQ(1, ctx.requiredSignatures);
  CHECK_EQ(0, ctx.readOnlySignedAccounts);
  CHECK_EQ(1, ctx.readOnlyUnsignedAccounts);
}

TEST_CASE("Test getLatestBlock") {
  auto connection = solana::rpc::Connection();
  auto blockHash = connection.getLatestBlockhash();
  CHECK(!blockHash.publicKey.toBase58().empty());
  CHECK_GT(blockHash.lastValidBlockHeight, 0);
}
TEST_CASE("MangoAccount is correctly created") {
  const auto key = solana::PublicKey::fromBase58(
      "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW");
  auto connection = solana::rpc::Connection(mango_v3::DEVNET.endpoint);
  // Test prefetched account info
  const auto mangoAccountInfo =
      connection.getAccountInfo<mango_v3::MangoAccountInfo>(key)
          .value.value()
          .data;
  const auto& mangoAccount = mango_v3::MangoAccount(mangoAccountInfo);
  CHECK(!(mangoAccount.mangoAccountInfo.owner == solana::PublicKey::empty()));
  // Test fetching account info in construction
  REQUIRE_NOTHROW(mango_v3::MangoAccount(key, connection));
  const auto& account = mango_v3::MangoAccount(key, connection);
  CHECK(!(account.mangoAccountInfo.owner == solana::PublicKey::empty()));
}
TEST_CASE("Test getMultipleAccountsInfo") {
  // Existing accounts
  std::vector<solana::PublicKey> accounts{
      solana::PublicKey::fromBase58(
          "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW"),
      solana::PublicKey::fromBase58(
          "DRUZRfLQtki4ZYvRXhi5yGmyqCf6iMfTzxtBpxo6rbHu"),
  };
  const auto connection = solana::rpc::Connection(mango_v3::DEVNET.endpoint);
  auto accountInfos =
      connection.getMultipleAccountsInfo<mango_v3::MangoAccountInfo>(accounts)
          .value;
  // query and result accounts should be same in number
  CHECK_EQ(accountInfos.size(), accounts.size());
  // Check AccountInfo is non-empty
  for (const auto& accountInfo : accountInfos) {
    CHECK(!(accountInfo.value().owner == solana::PublicKey::empty()));
  }
  // Introduce an account that doesn't exist
  accounts.push_back(solana::PublicKey::fromBase58(
      "9aZg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW"));
  accountInfos =
      connection.getMultipleAccountsInfo<mango_v3::MangoAccountInfo>(accounts)
          .value;
  CHECK_EQ(accountInfos.size(), accounts.size());
  // TODO: check for null pub key
}
TEST_CASE("Empty MangoAccount") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/empty";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 0);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 0);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 100);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 100);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 0);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 0);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}

TEST_CASE("1deposit") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/1deposit";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 37904260000.059059);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 42642292500.066528);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 100);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 100);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 47380.324999999997);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 0);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account1") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account1";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);

  auto openOrders3 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders3.json");
  auto openOrders6 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders6.json");
  auto openOrders7 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders7.json");

  auto get_address = [](const std::string& path) -> std::string {
    std::ifstream fileStream(path);
    std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
    auto response = json::parse(fileContent);
    return response["address"];
  };

  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders3.json")] =
      openOrders3;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders6.json")] =
      openOrders6;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders7.json")] =
      openOrders7;

  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");

  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 454884281.1552062);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 901472688.63722587);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 10.488604676089253);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 20.785925232226532);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 1348.2506615888819);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 3.2167149014445613);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account2") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account2";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);

  auto openOrders2 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders2.json");
  auto openOrders3 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders3.json");

  auto get_address = [](const std::string& path) -> std::string {
    std::ifstream fileStream(path);
    std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
    auto response = json::parse(fileContent);
    return response["address"];
  };

  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders2.json")] =
      openOrders2;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders3.json")] =
      openOrders3;

  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");

  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 7516159604.8491831);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 9618709877.4511909);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 24.806800043657162);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 31.746187568175088);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 11721.356691426183);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 3.5633861120422559);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account3") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account3";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 341025333625.51855);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 683477170424.20337);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 4.5265201884564732);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 9.5039735307640427);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 1025929.0072220544);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 6.501574727884357);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account4") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account4";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), -848086876487.04956);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), -433869053006.07361);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), -9.3065535308756608);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), -4.9878179847269166);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), -19651.229526046634);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), -421.56937094643047);
  CHECK_EQ(mangoAccount.isLiquidatable(mangoGroup, mangoCache), true);
}
TEST_CASE("account5") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account5";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);

  auto openOrders0 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders0.json");
  auto openOrders1 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders1.json");
  auto openOrders2 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders2.json");
  auto openOrders3 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders3.json");
  auto openOrders8 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders8.json");

  auto get_address = [](const std::string& path) -> std::string {
    std::ifstream fileStream(path);
    std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
    auto response = json::parse(fileContent);
    return response["address"];
  };

  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders0.json")] =
      openOrders0;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders1.json")] =
      openOrders1;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders2.json")] =
      openOrders2;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders3.json")] =
      openOrders3;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders8.json")] =
      openOrders8;

  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");

  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 15144959918141.092);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 15361719060997.684);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 878.88913077823327);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 946.44498820887998);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 15578478.173374372);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 0.098840765602176361);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account6") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account6";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);

  auto openOrders0 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders0.json");
  auto openOrders1 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders1.json");
  auto openOrders2 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders2.json");
  auto openOrders3 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders3.json");
  auto openOrders8 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders8.json");

  auto get_address = [](const std::string& path) -> std::string {
    std::ifstream fileStream(path);
    std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
    auto response = json::parse(fileContent);
    return response["address"];
  };

  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders0.json")] =
      openOrders0;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders1.json")] =
      openOrders1;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders2.json")] =
      openOrders2;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders3.json")] =
      openOrders3;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders8.json")] =
      openOrders8;

  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");

  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 14480970069238.33686487450164648294);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 15030566251990.17026082618337312624);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 215.03167137713001);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 236.77769605824432);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 15580162.407819409);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 0.079138709899027049);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account7") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account7";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);

  auto openOrders3 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders3.json");

  auto get_address = [](const std::string& path) -> std::string {
    std::ifstream fileStream(path);
    std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
    auto response = json::parse(fileContent);
    return response["address"];
  };

  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders3.json")] =
      openOrders3;

  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");

  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 16272272.28055547965738014682);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 16649749.17384252860704663135);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 359.23329723261616663876);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 400.98177879921832);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 17.027225950904331);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 0.2216901954540127);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account8") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account8";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);

  auto openOrders3 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders3.json");

  auto get_address = [](const std::string& path) -> std::string {
    std::ifstream fileStream(path);
    std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
    auto response = json::parse(fileContent);
    return response["address"];
  };

  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders3.json")] =
      openOrders3;

  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");

  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 337240882.73863375);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 496326340.62213475);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 36.051471007119673);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 53.05790488301021);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 655.41179779906793);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 1.4272596009734642);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}
TEST_CASE("account9") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account9";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);

  auto openOrders1 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders1.json");
  auto openOrders5 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders5.json");
  auto openOrders6 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders6.json");
  auto openOrders10 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders10.json");
  auto openOrders11 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders11.json");
  auto openOrders12 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders12.json");
  auto openOrders13 =
      solana::rpc::fromFile<serum_v3::OpenOrders>(path + "/openorders13.json");

  auto get_address = [](const std::string& path) -> std::string {
    std::ifstream fileStream(path);
    std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
    auto response = json::parse(fileContent);
    return response["address"];
  };

  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders1.json")] =
      openOrders1;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders5.json")] =
      openOrders5;
  mangoAccount.spotOpenOrdersAccounts[get_address(path + "/openorders6.json")] =
      openOrders6;
  mangoAccount
      .spotOpenOrdersAccounts[get_address(path + "/openorders10.json")] =
      openOrders10;
  mangoAccount
      .spotOpenOrdersAccounts[get_address(path + "/openorders11.json")] =
      openOrders11;
  mangoAccount
      .spotOpenOrdersAccounts[get_address(path + "/openorders12.json")] =
      openOrders12;
  mangoAccount
      .spotOpenOrdersAccounts[get_address(path + "/openorders13.json")] =
      openOrders13;

  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");

  auto initHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.to_double(), 96257596.932942361);
  auto maintHealth = mangoAccount.getHealth(mangoGroup, mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.to_double(), 511619124.36291981);
  auto initRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.to_double(), 2.9769382434196245);
  auto maintRatio = mangoAccount.getHealthRatio(mangoGroup, mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.to_double(), 17.211269135610507);
  auto value = mangoAccount.computeValue(mangoGroup, mangoCache);
  CHECK_EQ(value.to_double(), 926.9805324031521);
  auto leverage = mangoAccount.getLeverage(mangoGroup, mangoCache);
  CHECK_EQ(leverage.to_double(), 3.919442838288937);
  CHECK_FALSE(mangoAccount.isLiquidatable(mangoGroup, mangoCache));
}

TEST_CASE("getVersion") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto version = connection.getVersion();
  boost::regex expression{"\\d+\\.\\d+\\.\\d+"};
  CHECK_EQ(boost::regex_match(version.solana_core, expression), true);
  CHECK_GT(version.feature_set, 0);
}

TEST_CASE("getFirstAvailableBlock") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto firstAvailableBlock = connection.getFirstAvailableBlock();
  CHECK_GT(firstAvailableBlock, 0);
}

TEST_CASE("getSlot") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto slot1 =
      connection.getSlot(solana::GetSlotConfig{solana::Commitment::FINALIZED});
  const auto slot2 =
      connection.getSlot(solana::GetSlotConfig{solana::Commitment::CONFIRMED});
  const auto slot3 =
      connection.getSlot(solana::GetSlotConfig{solana::Commitment::PROCESSED});
  CHECK_LT(slot1, slot2);
  CHECK_LT(slot2, slot3);
}

TEST_CASE("getSlotLeader") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto slotLeader = connection.getSlotLeader();
  boost::regex expression{"^(?!.*[0OlI_])\\w*$"};
  CHECK_EQ(boost::regex_match(slotLeader, expression), true);
}

TEST_CASE("minimumLedgerSlot") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto slot = connection.minimumLedgerSlot();
  CHECK_GT(slot, 0);
}

TEST_CASE("getGenesisHash") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto hash = connection.getGenesisHash();
  CHECK_EQ(hash, DEVNET_GENESIS_HASH);
}

TEST_CASE("getEpochSchedule") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto epochschedule = connection.getEpochSchedule();
  CHECK_GE(epochschedule.firstNormalEpoch, 0);
  CHECK_GE(epochschedule.firstNormalSlot, 0);
  CHECK_GE(epochschedule.leaderScheduleSlotOffset, 0);
  CHECK_GE(epochschedule.slotsPerEpoch, 0);

  struct solana::EpochSchedule epochschedule2;
  epochschedule2.firstNormalEpoch = 14;
  epochschedule2.firstNormalSlot = 524256;
  epochschedule2.leaderScheduleSlotOffset = 432000;
  epochschedule2.slotsPerEpoch = 432000;
  epochschedule2.warmup = true;

  CHECK_EQ(epochschedule2.getEpoch(35), 1);

  CHECK((epochschedule2.getEpochAndSlotIndex(35)[0] == 1 &&
         epochschedule2.getEpochAndSlotIndex(35)[1] == 3));

  CHECK_EQ(epochschedule2.getEpoch(epochschedule2.firstNormalSlot +
                                   3 * epochschedule2.slotsPerEpoch + 12345),
           17);
  CHECK((epochschedule2.getEpochAndSlotIndex(epochschedule2.firstNormalSlot +
                                             3 * epochschedule2.slotsPerEpoch +
                                             12345)[0] == 17 &&
         epochschedule2.getEpochAndSlotIndex(epochschedule2.firstNormalSlot +
                                             3 * epochschedule2.slotsPerEpoch +
                                             12345)[1] == 12345));

  CHECK_EQ(epochschedule2.getSlotsInEpoch(4), 512);
  CHECK_EQ(epochschedule2.getFirstSlotInEpoch(2), 96);
  CHECK_EQ(epochschedule2.getLastSlotInEpoch(2), 223);
  CHECK_EQ(epochschedule2.getFirstSlotInEpoch(16),
           epochschedule2.firstNormalSlot + 2 * epochschedule2.slotsPerEpoch);
  CHECK_EQ(
      epochschedule2.getLastSlotInEpoch(16),
      epochschedule2.firstNormalSlot + 3 * epochschedule2.slotsPerEpoch - 1);
}

TEST_CASE("stakeactivation") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto stakeactivation =
      connection.getStakeActivation(solana::PublicKey::fromBase58(
          "CYRJWqiSjLitBAcRxPvWpgX3s5TvmN2SuRY3eEYypFvT"));
  CHECK_GE(stakeactivation.active, 0);
  CHECK_GE(stakeactivation.inactive, 0);
  CHECK((stakeactivation.state == "active" ||
         stakeactivation.state == "inactive" ||
         stakeactivation.state == "activating" ||
         stakeactivation.state == "deactivating"));
}

TEST_CASE("getInflationGovernor") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto inflationgovernor = connection.getInflationGovernor(
      solana::commitmentconfig{solana::Commitment::FINALIZED});
  CHECK_GE(inflationgovernor.foundation, 0);
  CHECK_GE(inflationgovernor.foundationTerm, 0);
  CHECK_GE(inflationgovernor.initial, 0);
  CHECK_EQ(inflationgovernor.taper, 0.15);
  CHECK_EQ(inflationgovernor.terminal, 0.015);
}

TEST_CASE("getTransactionCount") {
  const solana::Keypair keyPair = solana::Keypair::fromFile(KEY_PAIR_FILE);
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto TransactionCount = connection.getTransactionCount(
      solana::GetSlotConfig{solana::Commitment::CONFIRMED});
  CHECK_GT(TransactionCount, 0);

  // create a compiled transaction
  auto recentBlockHash = connection.getLatestBlockhash();
  const solana::PublicKey memoProgram =
      solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
  const std::string memo = "Hello";
  const solana::CompiledInstruction ix = {
      1, {}, std::vector<uint8_t>(memo.begin(), memo.end())};

  const solana::CompiledTransaction compiledTx = {
      recentBlockHash, {keyPair.publicKey, memoProgram}, {ix}, 1, 0, 1};

  const std::string transactionSignature =
      connection.sendTransaction(keyPair, compiledTx);
  uint8_t retries = 140;
  bool confirmed = false;
  confirmed = connection.confirmTransaction(
      transactionSignature, solana::Commitment::CONFIRMED, retries);
  const auto TransactionCount2 = connection.getTransactionCount(
      solana::GetSlotConfig{solana::Commitment::CONFIRMED});
  CHECK_GT(TransactionCount2, TransactionCount);
}

TEST_CASE("getEpochInfo") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto epochInfo = connection.getEpochInfo();
  CHECK_GT(epochInfo.absoluteSlot, 0);
  CHECK_GT(epochInfo.blockHeight, 0);
  CHECK_GT(epochInfo.epoch, 0);
  CHECK_GT(epochInfo.slotIndex, 0);
  CHECK_EQ(epochInfo.slotsInEpoch, 432000);
  CHECK_GT(epochInfo.transactionCount, 0);
}

TEST_CASE("getMinimumBalanceForRentExemption") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto minimumBalance = connection.getMinimumBalanceForRentExemption(
      512, solana::commitmentconfig{solana::Commitment::FINALIZED});
  CHECK_GE(minimumBalance, 0);
}

TEST_CASE("getMinimumBalanceForRentExemption") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto minimumBalance = connection.getMinimumBalanceForRentExemption(
      512, solana::commitmentconfig{solana::Commitment::FINALIZED});
  CHECK_GE(minimumBalance, 0);
}

TEST_CASE("getBlockTime") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto slot = connection.getFirstAvailableBlock();
  const auto BlockTime = connection.getBlockTime(slot);
  CHECK_GT(BlockTime, 0);
}

TEST_CASE("getClusterNodes") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto clusterNodes = connection.getClusterNodes();
  CHECK_GT(clusterNodes.size(), 0);
}

TEST_CASE("getFeeForMessage") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto FeeForMessage = connection.getFeeForMessage(
      "AQABAgIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEBAQAA");
  if (FeeForMessage.value.has_value()) {
    CHECK_GT(FeeForMessage.value.value(), 0);
  }
}

TEST_CASE("getLargestAccount") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto LargestAccount = connection.getLargestAccounts();
  CHECK_EQ(LargestAccount.value.size(), 20);
}

TEST_CASE("getRecentPerformanceSamples") {
  const auto connection = solana::rpc::Connection(solana::DEVNET);
  const auto RecentPerformanceSamples =
      connection.getRecentPerformanceSamples(4);
  CHECK_EQ(RecentPerformanceSamples[0].samplePeriodSecs, 60);
}
