#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "mango_account.hpp"

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
    std::ifstream fixture(resources_dir + "/base58/" + bs58, ios::binary);
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
  CHECK_EQ((int)round(event->makerFee.toDouble() * 10000), -4);
  CHECK_EQ(event->bestInitial, 0);
  CHECK_EQ(event->makerTimestamp, 1644182623);
  CHECK_EQ(event->taker.toBase58(),
           "2eTob7jrhKeHNhkK1jTfS3kZYdtNQS1VF7LETom6YHjJ");
  CHECK_EQ(to_string(event->takerOrderId), "7484028538144702206551329");
  CHECK_EQ(event->takerClientOrderId, 0);
  CHECK_EQ((int)round(event->takerFee.toDouble() * 10000), 5);
  CHECK_EQ(event->price, 415858);
  CHECK_EQ(event->quantity, 1);
}

TEST_CASE("compile memo transaction") {
  const solana::PublicKey recentBlockhash = {};
  const auto feePayer = solana::PublicKey::fromBase58(
      "8K4Exjnvs3ZJQDE78zmFoax5Sh4cEVdbk1D1r17Wxuud");
  const auto memoProgram =
      solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
  const std::string memo = "Hello \xF0\x9F\xA5\xAD";
  const solana::Instruction ix = {
      memoProgram, {}, std::vector<uint8_t>(memo.begin(), memo.end())};
  const auto ctx = solana::CompiledTransaction::fromInstructions(
      {ix}, feePayer, recentBlockhash);

  CHECK_EQ(recentBlockhash, ctx.recentBlockhash);
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
  const std::string& key = "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW";
  auto connection = solana::rpc::Connection(mango_v3::DEVNET.endpoint);
  // Test prefetched account info
  const auto& mangoAccountInfo =
      connection.getAccountInfo<mango_v3::MangoAccountInfo>(key);
  const auto& mangoAccount = mango_v3::MangoAccount(mangoAccountInfo);
  CHECK(!mangoAccount.accountInfo.owner.toBase58().empty());
  // Test fetching account info in construction
  REQUIRE_NOTHROW(mango_v3::MangoAccount(key, connection));
  const auto& account = mango_v3::MangoAccount(key, connection);
  CHECK(!account.accountInfo.owner.toBase58().empty());
}
TEST_CASE("Test getMultipleAccounts") {
  // Existing accounts
  std::vector<std::string> accounts{
      "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW",
      "DRUZRfLQtki4ZYvRXhi5yGmyqCf6iMfTzxtBpxo6rbHu",
  };
  auto connection = solana::rpc::Connection(mango_v3::DEVNET.endpoint);
  auto accountInfoMap =
      connection.getMultipleAccounts<mango_v3::MangoAccountInfo>(accounts);
  REQUIRE_EQ(accountInfoMap.size(), accounts.size());
  // Check results have the initial pubKeys
  auto it = accountInfoMap.find(accounts[0]);
  CHECK_NE(it, accountInfoMap.end());
  it = accountInfoMap.find(accounts[1]);
  CHECK_NE(it, accountInfoMap.end());
  // Check AccountInfo is non-empty
  for (const auto& [pubKey, accountInfo] : accountInfoMap) {
    auto owner = accountInfo.owner;
    CHECK(!(owner == solana::PublicKey::empty()));
  }
  // Introduce an account that doesn't exist
  accounts.push_back("9aZg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW");
  accountInfoMap =
      connection.getMultipleAccounts<mango_v3::MangoAccountInfo>(accounts);
  REQUIRE_NE(accountInfoMap.size(), accounts.size());
  it = accountInfoMap.find("9aZg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW");
  CHECK_EQ(it, accountInfoMap.end());
}
TEST_CASE("Empty MangoAccount") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/empty";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 0);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 0);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 100);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 100);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}

TEST_CASE("1deposit") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/1deposit";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 37904260000.05905822642118252475);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 42642292500.06652466908819931746);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 100);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 100);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account1") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account1";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));

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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 454884281.15520619643754685058);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 901472688.63722587052636470162);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 10.48860467608925262084);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 20.785925232226531989);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account2") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account2";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));

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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 7516159604.84918334545095675026);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 9618709877.45119083596852505025);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 24.80680004365716229131);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 31.74618756817508824497);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account3") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account3";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 341025333625.51856223547208912805);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 683477170424.20340250929429970483);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 4.52652018845647319267);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 9.50397353076404272088);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account4") {
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account4";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), -848086876487.04950427436299875694);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), -433869053006.07361789143756070075);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), -9.30655353087566084014);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), -4.98781798472691662028);
  CHECK_EQ(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache), true);
}
TEST_CASE("account5") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account5";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));

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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 15144959918141.09175135195858530324);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 15361719060997.68276021614036608298);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 878.88913077823325181726);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 946.44498820888003365326);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account6") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account6";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));

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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 14480970069238.33686487450164648294);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 15030566251990.17026082618337312624);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 215.03167137712999590349);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 236.77769605824430243501);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account7") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account7";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));

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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 16272272.28055547965738014682);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 16649749.17384252860704663135);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 359.23329723261616663876);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 400.98177879921834687593);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account8") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account8";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));

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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 337240882.73863372865950083224);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 496326340.62213476397751321656);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 36.05147100711967311781);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 53.05790488301020957351);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}
TEST_CASE("account9") {
  using json = nlohmann::json;
  std::string resources_dir = FIXTURES_DIR;
  auto path = resources_dir + "/mango_v3/account9";
  auto mangoGroup =
      solana::rpc::fromFile<mango_v3::MangoGroup>(path + "/group.json");
  auto mangoAccount =
      mango_v3::MangoAccount(solana::rpc::fromFile<mango_v3::MangoAccountInfo>(
          path + "/account.json"));

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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth.toDouble(), 96257596.93294236504926786324);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth.toDouble(), 511619124.36291981710078502488);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio.toDouble(), 2.97693824341962454127);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio.toDouble(), 17.21126913561050741919);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}