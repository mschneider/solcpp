#pragma once
#include <spdlog/spdlog.h>

#include <atomic>
#include <functional>
#include <mutex>

#include "subscriptions/bookSide.hpp"

namespace mango_v3 {
class orderbook {
 public:
  orderbook(subscription::bookSide& bids, subscription::bookSide& asks)
      : bids(bids), asks(asks) {
    bids.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
    asks.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
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
    snapshot rx;
    rx.highestBid = bids.getBestPrice();
    rx.lowestAsk = asks.getBestPrice();
    if (rx.valid()) {
      rx.midPoint = (rx.lowestAsk + rx.highestBid) / 2;
      rx.spreadBps =
          (((double)rx.lowestAsk - rx.highestBid) * 10000) / rx.midPoint;
      latestBook.store(rx);
      onUpdateCb();
    }
  }

  bool valid() const { return latestBook.load().valid(); }
  uint64_t getHighestBid() const { return latestBook.load().highestBid; }
  uint64_t getLowestAsk() const { return latestBook.load().lowestAsk; }
  uint64_t getMidPoint() const { return latestBook.load().midPoint; }
  double getSpreadBps() const { return latestBook.load().spreadBps; }

  uint64_t getDepth(int8_t percent) {
    auto price = (latestBook.load().midPoint * (100 + percent)) / 100;
    return (percent > 0) ? asks.getVolume<std::less_equal<uint64_t>>(price)
                         : bids.getVolume<std::greater_equal<uint64_t>>(price);
  }

 private:
  struct snapshot {
    uint64_t highestBid;
    uint64_t lowestAsk;
    uint64_t midPoint;
    double spreadBps;

    bool valid() const {
      return ((highestBid && lowestAsk) && (lowestAsk > highestBid)) ? true
                                                                     : false;
    }
  };

  std::atomic<snapshot> latestBook;
  std::function<void()> onUpdateCb;
  std::mutex callbackMtx;
  subscription::bookSide& bids;
  subscription::bookSide& asks;
};
}  // namespace mango_v3
