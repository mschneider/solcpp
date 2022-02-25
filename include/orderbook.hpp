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
      rx.spreadBsp =
          (((double)rx.lowestAsk - rx.highestBid) * 10000) / rx.midPoint;
      latestBook.store(rx);
      onUpdateCb();
    }
  }

  bool valid() const { return latestBook.load().valid(); }
  uint64_t getHighestBid() const { return latestBook.load().highestBid; }
  uint64_t getLowestAsk() const { return latestBook.load().lowestAsk; }
  uint64_t getMidPoint() const { return latestBook.load().midPoint; }
  double getSpreadBsp() const { return latestBook.load().spreadBsp; }

 private:
  struct snapshot {
    uint64_t highestBid;
    uint64_t lowestAsk;
    uint64_t midPoint;
    double spreadBsp;

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
