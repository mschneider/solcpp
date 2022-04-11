#include <spdlog/spdlog.h>

#include "mango_v3.hpp"

using json = nlohmann::json;

int main() {
  const auto& config = mango_v3::MAINNET;
  auto connection = solana::rpc::Connection(config.endpoint);
  const auto& mangoAccountInfo =
      connection.getAccountInfo<mango_v3::MangoAccountInfo>(
          "F3TTrgxjrkAHdS9zEidtwU5VXyvMgr5poii4HYatZheH");
  mango_v3::MangoAccount mangoAccount =
      mango_v3::MangoAccount(mangoAccountInfo);
  auto openOrders = mangoAccount.loadOpenOrders(connection);
  auto group = connection.getAccountInfo<mango_v3::MangoGroup>(config.group);
  auto cache = mango_v3::loadCache(connection, group.mangoCache.toBase58());
  spdlog::info("Owner: ", mangoAccountInfo.owner.toBase58());
  auto maintHealth =
      mangoAccount.getHealth(&group, &cache, mango_v3::HealthType::Maint);
  auto initHealth =
      mangoAccount.getHealth(&group, &cache, mango_v3::HealthType::Init);
  auto maintHealthRatio =
      mangoAccount.getHealthRatio(&group, &cache, mango_v3::HealthType::Maint);
  spdlog::info("Maint Health Ratio: {:.4f}", maintHealthRatio.toDouble());
  spdlog::info("Maint Health: {:.4f}", maintHealth.toDouble());
  spdlog::info("Init Health: {:.4f}", initHealth.toDouble());
  spdlog::info("---OpenOrders:{}---", openOrders.size());
  for (auto& openOrder : openOrders) {
    spdlog::info("Address: {}", openOrder.first);
    spdlog::info("Owner: {}", openOrder.second.owner.toBase58());
    spdlog::info("Market: {}", openOrder.second.market.toBase58());
    spdlog::info("baseTokenFree: {}", openOrder.second.baseTokenFree);
    spdlog::info("baseTokenTotal: {}", openOrder.second.baseTokenTotal);
    spdlog::info("quoteTokenFree: {}", openOrder.second.quoteTokenFree);
    spdlog::info("quoteTokenTotal: {}", openOrder.second.quoteTokenTotal);
  }
  // TODO: #13
}