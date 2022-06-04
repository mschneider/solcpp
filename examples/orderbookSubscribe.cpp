#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <mutex>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"
#include "subscriptions/accountSubscriber.hpp"
#include "subscriptions/orderbook.hpp"

class updateLogger {
 public:
  updateLogger(
      mango_v3::subscription::orderbook& orderbook,
      mango_v3::subscription::accountSubscriber<mango_v3::Trades>& trades,
      const mango_v3::NativeToUi& nativeToUi)
      : orderbook(orderbook), trades(trades), nativeToUi(nativeToUi) {
    orderbook.registerUpdateCallback(std::bind(&updateLogger::logUpdate, this));
    trades.registerUpdateCallback(std::bind(&updateLogger::logUpdate, this));
    orderbook.registerCloseCallback(std::bind(&updateLogger::abort, this));
    trades.registerCloseCallback(std::bind(&updateLogger::abort, this));
  }

  void start() {
    orderbook.subscribe();
    trades.subscribe();
  }

  void logUpdate() {
    std::scoped_lock lock(logMtx);
    auto level1Snapshot = orderbook.getLevel1();
    if (level1Snapshot) {
      if (level1Snapshot->valid()) {
        spdlog::info("============Update============");
        auto lastTrade = trades.getAccount()->getLastTrade();
        if (lastTrade) {
          spdlog::info("Last trade: price {:.2f}, quantity {:.2f}",
                       nativeToUi.getPrice(lastTrade->price),
                       nativeToUi.getQuantity(lastTrade->quantity));
        } else {
          spdlog::info("No trade since the subscription started");
        }
        spdlog::info("Bid-Ask ({:.2f}) {:.2f}-{:.2f} ({:.2f})",
                     nativeToUi.getQuantity(level1Snapshot->highestBidSize),
                     nativeToUi.getPrice(level1Snapshot->highestBid),
                     nativeToUi.getPrice(level1Snapshot->lowestAsk),
                     nativeToUi.getQuantity(level1Snapshot->lowestAskSize));
        spdlog::info("MidPrice: {:.2f}",
                     nativeToUi.getPrice(level1Snapshot->midPoint));
        spdlog::info("Spread: {:.2f} bps", level1Snapshot->spreadBps);

        constexpr auto depth = 2;
        spdlog::info("Market depth -{}%: {}", depth,
                     orderbook.getDepth(-depth));
        spdlog::info("Market depth +{}%: {}", depth, orderbook.getDepth(depth));
      }
    }
  }

  void abort() {
    spdlog::error("websocket subscription error");
    throw std::runtime_error("websocket subscription error");
  }

 private:
  mango_v3::subscription::orderbook& orderbook;
  mango_v3::subscription::accountSubscriber<mango_v3::Trades>& trades;
  const mango_v3::NativeToUi& nativeToUi;
  std::mutex logMtx;
};

int main() {
  using namespace mango_v3;
  const auto& config = MAINNET;
  const solana::rpc::Connection solCon;
  const auto group = solCon.getAccountInfo<MangoGroup>(config.group);

  const auto symbolIt =
      std::find(config.symbols.begin(), config.symbols.end(), "SOL");
  const auto marketIndex = symbolIt - config.symbols.begin();
  assert(config.symbols[marketIndex] == "SOL");

  const auto perpMarketPk = group.perpMarkets[marketIndex].perpMarket;
  auto market = solCon.getAccountInfo<PerpMarket>(perpMarketPk.toBase58());
  assert(market.mangoGroup.toBase58() == config.group);

  subscription::accountSubscriber<Trades> trades(market.eventQueue.toBase58());
  subscription::orderbook book(market);

  mango_v3::NativeToUi nativeToUi(market.quoteLotSize, market.baseLotSize,
                                  group.tokens[QUOTE_INDEX].decimals,
                                  group.tokens[marketIndex].decimals);
  updateLogger logger(book, trades, nativeToUi);
  logger.start();

  using namespace std::literals::chrono_literals;
  while (true) {
    std::this_thread::sleep_for(100s);
  }

  return 0;
}
