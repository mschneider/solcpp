#pragma once

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <vector>

#include "mango_v3.hpp"
#include "solana.hpp"
#include "wssSubscriber.hpp"

namespace mango_v3 {
namespace subscription {

using json = nlohmann::json;

class bookSide {
 public:
  bookSide(Side side, const std::string& account)
      : side(side), wssConnection(account) {}

  void registerUpdateCallback(std::function<void()> callback) {
    updateCallback = callback;
  }

  void subscribe() {
    wssConnection.registerOnMessageCallback(
        std::bind(&bookSide::onMessage, this, std::placeholders::_1));
    wssConnection.start();
  }

  uint64_t getBestPrice() const {
    std::scoped_lock lock(ordersMtx);
    return (!orders.empty()) ? orders.back().price : 0;
  }

  uint64_t getDepth(uint8_t percent) const {
    std::scoped_lock lock(ordersMtx);
    return 0;
  }  // todo: add depth logic

 private:
  wssSubscriber wssConnection;
  const Side side;

  struct order {
    order(uint64_t price, uint64_t quantity)
        : price(price), quantity(quantity) {}

    bool operator<(const order& compare) {
      return (price < compare.price) ? true : false;
    }

    bool operator>(const order& compare) {
      return (price > compare.price) ? true : false;
    }

    uint64_t price;
    uint64_t quantity;
  };

  mutable std::mutex ordersMtx;
  std::vector<order> orders;
  std::function<void()> updateCallback;

  void onMessage(const json& parsedMsg) {
    // ignore subscription confirmation
    const auto itResult = parsedMsg.find("result");
    if (itResult != parsedMsg.end()) {
      spdlog::info("on_result {}", parsedMsg.dump());
      return;
    }

    const std::string encoded =
        parsedMsg["params"]["result"]["value"]["data"][0];

    const std::string decoded = solana::b64decode(encoded);
    if (decoded.size() != sizeof(BookSide))
      throw std::runtime_error("invalid response length " +
                               std::to_string(decoded.size()) + " expected " +
                               std::to_string(sizeof(BookSide)));

    BookSide bookSide;
    memcpy(&bookSide, decoded.data(), sizeof(decltype(bookSide)));

    auto iter = BookSide::iterator(side, bookSide);

    decltype(orders) newOrders;

    while (iter.stack.size() > 0) {
      if ((*iter).tag == NodeType::LeafNode) {
        const auto leafNode =
            reinterpret_cast<const struct LeafNode*>(&(*iter));
        const auto now = std::chrono::system_clock::now();
        const auto nowUnix =
            chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                .count();
        const auto isValid =
            !leafNode->timeInForce ||
            leafNode->timestamp + leafNode->timeInForce < nowUnix;
        if (isValid) {
          newOrders.emplace_back((uint64_t)(leafNode->key >> 64),
                                 leafNode->quantity);
        }
      }
      ++iter;
    }

    if (!newOrders.empty()) {
      {
        std::scoped_lock lock(ordersMtx);
        orders = std::move(newOrders);
        if (side == Side::Buy) {
          std::sort(orders.begin(), orders.end());
        } else {
          std::sort(orders.begin(), orders.end(), std::greater{});
        }
      }
      updateCallback();
    }
  }
};
}  // namespace subscription
}  // namespace mango_v3
