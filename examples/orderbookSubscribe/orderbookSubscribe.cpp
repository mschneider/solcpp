#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "bookSideSubscription.hpp"
#include "orderbook.hpp"
#include "mango_v3.hpp"
#include "solana.hpp"

int main() {
  const auto &config = mango_v3::MAINNET;
  // todo: make this const
  solana::rpc::Connection solCon;
  const auto group = solCon.getAccountInfo<mango_v3::MangoGroup>(config.group);

  const auto symbolIt =
      std::find(config.symbols.begin(), config.symbols.end(), "BTC");
  const auto marketIndex = symbolIt - config.symbols.begin();
  assert(config.symbols[marketIndex] == "BTC");

  const auto perpMarketPk = group.perpMarkets[marketIndex].perpMarket;
  auto market =
      solCon.getAccountInfo<mango_v3::PerpMarket>(perpMarketPk.toBase58());
  assert(market.mangoGroup.toBase58() == config.group);

  examples::bookSideSubscription bids(mango_v3::Buy, market.bids.toBase58());
  examples::bookSideSubscription asks(mango_v3::Sell, market.asks.toBase58());

  examples::orderbook book(bids, asks);

  while (true) {
  }
  return 0;
}
