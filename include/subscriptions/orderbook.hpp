#pragma once
#include <spdlog/spdlog.h>

#include <functional>

#include "mango_v3.hpp"

namespace mango_v3 {
namespace subscription {
class orderbook {
 public:
  orderbook(const std::string& bidsAccount, const std::string& asksAccount)
      : bids(bidsAccount, Buy), asks(asksAccount, Sell) {
    bids.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
    asks.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
  }

  template <typename func>
  void registerUpdateCallback(func&& callback) {
    onUpdateCb = std::forward<func>(callback);
  }

  template <typename func>
  void registerCloseCallback(func&& callback) {
    bids.registerCloseCallback(callback);
    asks.registerCloseCallback(std::forward<func>(callback));
  }

  void subscribe() {
    bids.subscribe();
    asks.subscribe();
  }

  void updateCallback() {
    L1Orderbook newL1;
    auto bestBid = bids.getAccount()->getBestOrder();
    auto bestAsk = asks.getAccount()->getBestOrder();
    if (bestBid && bestAsk) {
      newL1.highestBid = (uint64_t)(bestBid->key >> 64);
      newL1.highestBidSize = bestBid->quantity;
      newL1.lowestAsk = (uint64_t)(bestAsk->key >> 64);
      newL1.lowestAskSize = bestAsk->quantity;

      if (newL1.valid()) {
        newL1.midPoint = ((double)newL1.lowestAsk + newL1.highestBid) / 2;
        newL1.spreadBps =
            ((newL1.lowestAsk - newL1.highestBid) * 10000) / newL1.midPoint;
        level1 = std::make_shared<L1Orderbook>(std::move(newL1));
        onUpdateCb();
      }
    }
  }

  auto getLevel1() const { return level1; }

  auto getDepth(int8_t percent) {
    uint64_t depth = 0;
    if (level1) {
      auto price = (level1->midPoint * (100 + percent)) / 100;
      depth = (percent > 0) ? asks.getAccount()->getVolume(price)
                            : bids.getAccount()->getVolume(price);
    }
    return depth;
  }

 private:
  std::shared_ptr<L1Orderbook> level1;
  std::function<void()> onUpdateCb;
  subscription::accountSubscriber<BookSide> bids;
  subscription::accountSubscriber<BookSide> asks;
};
}  // namespace subscription
}  // namespace mango_v3
