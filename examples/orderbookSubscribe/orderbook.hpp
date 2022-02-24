#pragma once
#include <spdlog/spdlog.h>

#include "bookSideSubscription.hpp"

namespace examples {
class orderbook {
 public:
  orderbook(bookSideSubscription& bids, bookSideSubscription& asks)
      : bids(bids), asks(asks) {
    bids.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
    asks.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
    bids.subscribe();
    asks.subscribe();
  }

  void updateCallback() {
    latestBook.highestBid = bids.getBestPrice();
    latestBook.lowestAsk = asks.getBestPrice();
    if (latestBook.valid())
      spdlog::info("============{}============", __func__);
    latestBook.midPoint = (latestBook.lowestAsk + latestBook.highestBid) / 2;
    latestBook.spreadBsp =
        (((double)latestBook.lowestAsk - latestBook.highestBid) * 10000) /
        latestBook.midPoint;

    spdlog::info("highestBid: {}", latestBook.highestBid);
    spdlog::info("lowestAsk: {}", latestBook.lowestAsk);
    spdlog::info("MidPrice: {}", latestBook.midPoint);
    spdlog::info("Spread in basis points: {0:.2f}", latestBook.spreadBsp);
  }

 private:
  const bookSideSubscription& bids;
  const bookSideSubscription& asks;

  struct pxSnapshot {
    uint64_t highestBid;
    uint64_t lowestAsk;
    uint64_t midPoint;
    double spreadBsp;

    bool valid() {
      return ((highestBid && lowestAsk) && (lowestAsk > highestBid)) ? true
                                                                     : false;
    }
  };

  pxSnapshot latestBook;
};
}  // namespace examples
