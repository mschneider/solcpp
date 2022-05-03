#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "MangoAccount.hpp"

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
  auto mangoAccountInfo =
      connection.getAccountInfo<mango_v3::MangoAccountInfo>(key);
  const auto& mangoAccount = mango_v3::MangoAccount(mangoAccountInfo);
  CHECK(!(mangoAccount.mangoAccountInfo.owner == solana::PublicKey::empty()));
  // Test fetching account info in construction
  REQUIRE_NOTHROW(mango_v3::MangoAccount(key, connection));
  const auto& account = mango_v3::MangoAccount(key, connection);
  CHECK(!(account.mangoAccountInfo.owner == solana::PublicKey::empty()));
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
  auto accountInfo =
      solana::rpc::fromFile<mango_v3::MangoAccountInfo>(path + "/account.json");
  auto mangoAccount = mango_v3::MangoAccount(accountInfo);
  auto mangoCache =
      solana::rpc::fromFile<mango_v3::MangoCache>(path + "/cache.json");
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 0);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 0);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 100);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 100);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 0);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 0);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 37904260000.059052);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 42642292500.066513);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 100);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 100);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 47380.32499999999999928946);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 0);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 454884281.15520579);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 901472688.63722574);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 10.48860467608943);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 20.785925232226798);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 1348.2506615888833);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 3.2167149014445608);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 7516159604.8491821);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 9618709877.4511909);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 24.806800043657297);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 31.746187568175088);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 11721.356691426183);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 3.5633861120422576);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 341025333625.51721);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 683477170424.20276);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 4.5265201884565842);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 9.5039735307641315);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 1025929.0072220536);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 6.5015747278843659);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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
  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, -848086876487.0498);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, -433869053006.07324);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, -9.306553530875572);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, -4.9878179847267052);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, -19651.229526046664);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, -421.56937094642979);
  CHECK_EQ(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache), true);
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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 15144959918141.09175135195858530324);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 15361719060997.6826021614036608298);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 878.88913077823338);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 946.4449882088802);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 15578478.17337437);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 0.098840765602179497);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 14480970069238.33686487450164648294);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 15030566251990.17026082618337312624);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 215.03167137713018);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 236.77769605824452);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 15580162.407819403);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 0.079138709899027215);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 16272272.28055547965738014682);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 16649749.17384252860704663135);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 359.23329723261616663876);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 400.98177879921832);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 17.02722595090421);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 0.22169019545402435);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 337240882.73863387);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 496326340.62213492);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 36.051471007120028);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 53.057904883010345);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 655.41179779906815);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 1.4272596009734659);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
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

  auto initHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                           mango_v3::HealthType::Init);
  CHECK_EQ(initHealth, 96257596.932942599);
  auto maintHealth = mangoAccount.getHealth(&mangoGroup, &mangoCache,
                                            mango_v3::HealthType::Maint);
  CHECK_EQ(maintHealth, 511619124.36291969);
  auto initRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                               mango_v3::HealthType::Init);
  CHECK_EQ(initRatio, 2.9769382434197134);
  auto maintRatio = mangoAccount.getHealthRatio(&mangoGroup, &mangoCache,
                                                mango_v3::HealthType::Maint);
  CHECK_EQ(maintRatio, 17.211269135610863);
  auto value = mangoAccount.computeValue(&mangoGroup, &mangoCache);
  CHECK_EQ(value, 926.98053240315084);
  auto leverage = mangoAccount.getLeverage(&mangoGroup, &mangoCache);
  CHECK_EQ(leverage, 3.9194428382889464);
  CHECK_FALSE(mangoAccount.isLiquidatable(&mangoGroup, &mangoCache));
}