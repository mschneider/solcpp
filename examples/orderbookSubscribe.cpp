#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <mutex>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "mango_v3.hpp"
#include "orderbook.hpp"
#include "solana.hpp"
#include "subscriptions/bookSide.hpp"
#include "subscriptions/trades.hpp"

class updateLogger {
 public:
  updateLogger(mango_v3::orderbook& book,
               mango_v3::subscription::trades& trades)
      : book(book), trades(trades) {
    book.registerUpdateCallback(std::bind(&updateLogger::logUpdate, this));
    trades.registerUpdateCallback(std::bind(&updateLogger::logUpdate, this));
  }

  void start() {
    book.subscribe();
    trades.subscribe();
  }

  void logUpdate() {
    const std::scoped_lock lock(updateMtx);
    if (book.valid()) {
      spdlog::info("============Orderbook Update============");
      spdlog::info("Latest trade: {}", trades.getLastTrade()
                                           ? to_string(trades.getLastTrade())
                                           : "not received yet");
      spdlog::info("Bid-Ask {}-{}", book.getHighestBid(), book.getLowestAsk());
      spdlog::info("MidPrice: {}", book.getMidPoint());
      spdlog::info("Spread: {0:.2f} bps", book.getSpreadBps());

      auto depth = 2;
      spdlog::info("Market depth +{}%: {}", depth, book.getDepth(depth));
      spdlog::info("Market depth -{}%: {}", depth, book.getDepth(-depth));
    }
  }

 private:
  std::mutex updateMtx;
  mango_v3::orderbook& book;
  mango_v3::subscription::trades& trades;
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

  mango_v3::subscription::bookSide bids(mango_v3::Buy, market.bids.toBase58());
  mango_v3::subscription::bookSide asks(mango_v3::Sell, market.asks.toBase58());
  mango_v3::subscription::trades trades(market.eventQueue.toBase58());

  mango_v3::orderbook book(bids, asks);

  updateLogger logger(book, trades);
  logger.start();

  while (true) {
  }
  return 0;
}

// void updateReceived() {}
