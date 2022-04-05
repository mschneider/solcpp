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
  spdlog::info("Owner: ", mangoAccountInfo.owner.toBase58());
  ;
  auto openOrders = mangoAccount.loadOpenOrders(connection);
  spdlog::info("---OpenOrders:{}---", openOrders.size());
  for (auto& openOrder : openOrders) {
    spdlog::info("Address: {}", openOrder.first);
    spdlog::info("Owner: {}", openOrder.second.owner.toBase58());
    spdlog::info("Market: {}", openOrder.second.market.toBase58());
    spdlog::info("AccountFlags: {}", openOrder.second.accountFlags.to_string());
    spdlog::info("baseTokenFree: {}", openOrder.second.baseTokenFree);
    spdlog::info("baseTokenTotal: {}", openOrder.second.baseTokenTotal);
    spdlog::info("quoteTokenFree: {}", openOrder.second.quoteTokenFree);
    spdlog::info("quoteTokenTotal: {}", openOrder.second.quoteTokenTotal);
  }
  // TODO: #13
}