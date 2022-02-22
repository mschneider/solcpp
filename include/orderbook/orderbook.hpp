#pragma once
#include <spdlog/spdlog.h>

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
    const std::scoped_lock lock(callbackMtx);
    levelOne newL1;
    auto bestBid = bids.getBestOrder();
    auto bestAsk = asks.getBestOrder();
    newL1.highestBid = bestBid.price;
    newL1.highestBidSize = bestBid.quantity;
    newL1.lowestAsk = bestAsk.price;
    newL1.lowestAskSize = bestAsk.quantity;

    if (newL1.valid()) {
      newL1.midPoint = ((double)newL1.lowestAsk + newL1.highestBid) / 2;
      newL1.spreadBps =
          ((newL1.lowestAsk - newL1.highestBid) * 10000) / newL1.midPoint;
      {
        const std::scoped_lock lock(levelOneMtx);
        level1 = newL1;
      }
      onUpdateCb();
    }
  }

  levelOne getLevel1() const {
    const std::scoped_lock lock(levelOneMtx);
    return level1;
  }

  uint64_t getDepth(int8_t percent) {
    const std::scoped_lock lock(levelOneMtx);
    auto price = (level1.midPoint * (100 + percent)) / 100;
    return (percent > 0) ? asks.getVolume<std::less_equal<uint64_t>>(price)
                         : bids.getVolume<std::greater_equal<uint64_t>>(price);
  }

 private:
  levelOne level1;
  // todo:macos latomic not found issue, otherwise replace mtx with std::atomic
  mutable std::mutex levelOneMtx;
  std::function<void()> onUpdateCb;
  std::mutex callbackMtx;
  subscription::bookSide& bids;
  subscription::bookSide& asks;
};
}  // namespace orderbook
}  // namespace mango_v3
