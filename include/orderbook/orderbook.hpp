#pragma once
#include <spdlog/spdlog.h>

#include <atomic>
#include <functional>
#include <mutex>

#include "levelOne.hpp"
#include "orderbook/order.hpp"
#include "subscriptions/bookSide.hpp"

namespace mango_v3 {
namespace orderbook {
class book {
 public:
  book(subscription::bookSide& bids, subscription::bookSide& asks)
      : bids(bids), asks(asks) {
    bids.registerUpdateCallback(std::bind(&book::updateCallback, this));
    asks.registerUpdateCallback(std::bind(&book::updateCallback, this));
  }

  void registerUpdateCallback(std::function<void()> callback) {
    onUpdateCb = callback;
  }

  void subscribe() {
    bids.subscribe();
    asks.subscribe();
  }

  void updateCallback() {
    const std::lock_guard<std::mutex> lock(callbackMtx);
    levelOne newL1;
    auto bestBid = bids.getBestOrder();
    auto bestAsk = asks.getBestOrder();
    newL1.highestBid = bestBid.price;
    newL1.highestBidSize = bestBid.quantity;
    newL1.lowestAsk = bestAsk.price;
    newL1.lowestAskSize = bestAsk.quantity;

    if (newL1.valid()) {
      newL1.midPoint = (newL1.lowestAsk + newL1.highestBid) / 2;
      newL1.spreadBps = (((double)newL1.lowestAsk - newL1.highestBid) * 10000) /
                        newL1.midPoint;
      level1.store(newL1);
      onUpdateCb();
    }
  }

  levelOne getLevel1() const { return level1.load(); }

  uint64_t getDepth(int8_t percent) {
    auto price = (level1.load().midPoint * (100 + percent)) / 100;
    return (percent > 0) ? asks.getVolume<std::less_equal<uint64_t>>(price)
                         : bids.getVolume<std::greater_equal<uint64_t>>(price);
  }

 private:
  std::atomic<levelOne> level1;
  std::function<void()> onUpdateCb;
  std::mutex callbackMtx;
  subscription::bookSide& bids;
  subscription::bookSide& asks;
};
}  // namespace orderbook
}  // namespace mango_v3
