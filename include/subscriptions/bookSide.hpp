#pragma once

#include <spdlog/spdlog.h>

#include <algorithm>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <vector>

#include "mango_v3.hpp"
#include "orderbook/order.hpp"
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
    notifyCb = callback;
  }

  void subscribe() {
    wssConnection.registerOnMessageCallback(
        std::bind(&bookSide::onMessage, this, std::placeholders::_1));
    wssConnection.start();
  }

  orderbook::order getBestOrder() const {
    std::scoped_lock lock(ordersMtx);
    return (!orders.empty()) ? orders.front() : orderbook::order{0, 0};
  }

  template <typename Op>
  uint64_t getVolume(uint64_t price) const {
    Op operation;
    uint64_t volume = 0;
    std::scoped_lock lock(ordersMtx);
    for (auto&& order : orders) {
      if (operation(order.price, price)) {
        volume += order.quantity;
      } else {
        break;
      }
    }
    return volume;
  }

 private:
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
          std::sort(orders.begin(), orders.end(), std::greater{});
        } else {
          std::sort(orders.begin(), orders.end());
        }
      }
      notifyCb();
    }
  }

  wssSubscriber wssConnection;
  const Side side;
  mutable std::mutex ordersMtx;
  std::vector<orderbook::order> orders;
  std::function<void()> notifyCb;
};
}  // namespace subscription
}  // namespace mango_v3
