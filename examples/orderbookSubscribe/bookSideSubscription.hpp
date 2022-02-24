#pragma once

#include <spdlog/spdlog.h>

#include <functional>
#include <nlohmann/json.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"
#include "wssSubscriber.hpp"

namespace examples {

using json = nlohmann::json;

class bookSideSubscription {
 public:
  bookSideSubscription(mango_v3::Side side, const std::string& account)
      : side(side), wssConnection(account) {}

  void registerUpdateCallback(std::function<void()> callback) {
    updateCallback = callback;
  }

  void subscribe() {
    wssConnection.registerOnMessageCallback(std::bind(
        &bookSideSubscription::onMessage, this, std::placeholders::_1));
    wssConnection.start();
  }

  uint64_t getBestPrice() const { return bestPrice; }

 private:
  wssSubscriber wssConnection;
  const mango_v3::Side side;
  uint64_t bestPrice = 0;
  uint64_t quantity = 0;
  std::function<void()> updateCallback;

  // todo: move better here?
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
    if (decoded.size() != sizeof(mango_v3::BookSide))
      throw std::runtime_error("invalid response length " +
                               std::to_string(decoded.size()) + " expected " +
                               std::to_string(sizeof(mango_v3::BookSide)));

    mango_v3::BookSide bookSide;
    memcpy(&bookSide, decoded.data(), sizeof(decltype(bookSide)));

    auto iter = mango_v3::BookSide::iterator(side, bookSide);

    while (iter.stack.size() > 0) {
      if ((*iter).tag == mango_v3::NodeType::LeafNode) {
        const auto leafNode =
            reinterpret_cast<const struct mango_v3::LeafNode*>(&(*iter));
        const auto now = std::chrono::system_clock::now();
        const auto nowUnix =
            chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                .count();
        const auto isValid =
            !leafNode->timeInForce ||
            leafNode->timestamp + leafNode->timeInForce < nowUnix;
        if (isValid) {
          bestPrice = (uint64_t)(leafNode->key >> 64);
          quantity = leafNode->quantity;
          updateCallback();
          break;
        }
      }
      ++iter;
    }
  }

  void logBest() const {
    // todo: if decided to make public, have to add mtx/atomic to avoid data
    // race
    spdlog::info("{} best price: {} quantity: {}", side ? "asks" : "bids",
                 bestPrice, quantity);
  }
};
}  // namespace examples
