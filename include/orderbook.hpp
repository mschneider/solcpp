#pragma once
#include <spdlog/spdlog.h>

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
    latestBook.highestBid = bids.getBestPrice();
    latestBook.lowestAsk = asks.getBestPrice();
    if (latestBook.valid()) {
      latestBook.midPoint = (latestBook.lowestAsk + latestBook.highestBid) / 2;
      latestBook.spreadBsp =
          (((double)latestBook.lowestAsk - latestBook.highestBid) * 10000) /
          latestBook.midPoint;
    }
    onUpdateCb();
  }

  bool valid() const { return latestBook.valid(); }
  uint64_t getHighestBid() const { return latestBook.highestBid; }
  uint64_t getLowestAsk() const { return latestBook.lowestAsk; }
  uint64_t getMidPoint() const { return latestBook.midPoint; }
  double getSpreadBsp() const { return latestBook.spreadBsp; }

 private:
  std::function<void()> onUpdateCb;
  std::mutex callbackMtx;
  subscription::bookSide& bids;
  subscription::bookSide& asks;

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

  snapshot latestBook;
};
}  // namespace mango_v3
