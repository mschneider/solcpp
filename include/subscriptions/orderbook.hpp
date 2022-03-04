#pragma once
#include <spdlog/spdlog.h>

#include <functional>

#include "bookSide.hpp"
#include "orderbook/levelOne.hpp"
#include "orderbook/order.hpp"

namespace mango_v3 {
namespace subscription {
class orderbook {
 public:
  orderbook(const std::string& bidsAccount, const std::string& asksAccount)
      : bids(Buy, bidsAccount), asks(Sell, asksAccount) {
    bids.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
    asks.registerUpdateCallback(std::bind(&orderbook::updateCallback, this));
  }

  void registerUpdateCallback(std::function<void()> callback) {
    onUpdateCb = callback;
  }

  void registerCloseCallback(std::function<void()> callback) {
    bids.registerCloseCallback(callback);
    asks.registerCloseCallback(callback);
  }

  void subscribe() {
    bids.subscribe();
    asks.subscribe();
  }

  void updateCallback() {
    book::levelOne newL1;
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
        level1 = std::make_shared<book::levelOne>(std::move(newL1));
      onUpdateCb();
    }
  }

  auto getLevel1() const {
    return level1;
  }

  auto getDepth(int8_t percent) {
    auto price = (level1->midPoint * (100 + percent)) / 100;
    return (percent > 0) ? asks.getVolume(price) : bids.getVolume(price);
  }

 private:
  std::shared_ptr<book::levelOne> level1 = std::make_shared<book::levelOne>();
  std::function<void()> onUpdateCb;
  subscription::bookSide bids;
  subscription::bookSide asks;
};
}  // namespace subscription
}  // namespace mango_v3
