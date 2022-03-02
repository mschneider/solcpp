#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <mutex>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "mango_v3.hpp"
#include "orderbook/levelOne.hpp"
#include "solana.hpp"
#include "subscriptions/orderbook.hpp"
#include "subscriptions/trades.hpp"

class updateLogger {
 public:
  updateLogger(mango_v3::subscription::orderbook& orderbook,
               mango_v3::subscription::trades& trades)
      : orderbook(orderbook), trades(trades) {
    orderbook.registerUpdateCallback(std::bind(&updateLogger::logUpdate, this));
    trades.registerUpdateCallback(std::bind(&updateLogger::logUpdate, this));
    orderbook.registerCloseCallback(std::bind(&updateLogger::stop, this));
    trades.registerCloseCallback(std::bind(&updateLogger::stop, this));

    orderbook.subscribe();
    trades.subscribe();
  }

  bool isOk() {
    const std::scoped_lock lock(updateMtx);
    return !subscriptionError;
  }

  void logUpdate() {
    const std::scoped_lock lock(updateMtx);
    if (!subscriptionError) {
      auto level1Snapshot = orderbook.getLevel1();
      if (level1Snapshot.valid()) {
        spdlog::info("============Update============");
        spdlog::info("Latest trade: {}", trades.getLastTrade()
                                             ? to_string(trades.getLastTrade())
                                             : "not received yet");
        spdlog::info("Bid-Ask {}-{}", level1Snapshot.highestBid,
                     level1Snapshot.lowestAsk);
        spdlog::info("MidPrice: {}", level1Snapshot.midPoint);
        spdlog::info("Spread: {0:.2f} bps", level1Snapshot.spreadBps);

        constexpr auto depth = 2;
        spdlog::info("Market depth -{}%: {}", depth,
                     orderbook.getDepth(-depth));
        spdlog::info("Market depth +{}%: {}", depth, orderbook.getDepth(depth));
      }
    }
  }

  void stop() {
    const std::scoped_lock lock(updateMtx);
    spdlog::error("websocket subscription error");
    subscriptionError = true;
  }

 private:
  std::mutex updateMtx;
  mango_v3::subscription::orderbook& orderbook;
  mango_v3::subscription::trades& trades;
  bool subscriptionError = false;
};

int main() {
  const auto& config = mango_v3::MAINNET;
  const solana::rpc::Connection solCon;
  const auto group = solCon.getAccountInfo<mango_v3::MangoGroup>(config.group);

  const auto symbolIt =
      std::find(config.symbols.begin(), config.symbols.end(), "SOL");
  const auto marketIndex = symbolIt - config.symbols.begin();
  assert(config.symbols[marketIndex] == "SOL");

  const auto perpMarketPk = group.perpMarkets[marketIndex].perpMarket;
  auto market =
      solCon.getAccountInfo<mango_v3::PerpMarket>(perpMarketPk.toBase58());
  assert(market.mangoGroup.toBase58() == config.group);

  mango_v3::subscription::trades trades(market.eventQueue.toBase58());
  mango_v3::subscription::orderbook book(market.bids.toBase58(),
                                         market.asks.toBase58());

  updateLogger logger(book, trades);

  while (logger.isOk()) {
    std::this_thread::sleep_for(1s);
  }

  return 0;
}
