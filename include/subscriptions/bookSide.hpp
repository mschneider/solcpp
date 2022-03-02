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
      : bookSide_(side), wssConnection(account) {}

  void registerUpdateCallback(std::function<void()> callback) {
    notifyCb = callback;
  }

  void registerCloseCallback(std::function<void()> callback) {
    closeCb = callback;
  }

  void subscribe() {
    wssConnection.registerOnMessageCallback(
        std::bind(&bookSide::onMessage, this, std::placeholders::_1));
    wssConnection.registerOnCloseCallback(std::bind(&bookSide::onClose, this));
    wssConnection.start();
  }

  book::order getBestOrder() const { return bookSide_.getBestOrder(); }

  uint64_t getVolume(uint64_t price) const {
    return bookSide_.getVolume(price);
  }

 private:
  void onClose() {
    if (closeCb) {
      closeCb();
    }
  }

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
    if (bookSide_.update(decoded)) {
      if (notifyCb) {
        notifyCb();
      }
    }
  }

  wssSubscriber wssConnection;
  BookSide bookSide_;
  std::function<void()> notifyCb;
  std::function<void()> closeCb;
};
}  // namespace subscription
}  // namespace mango_v3
