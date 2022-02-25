#pragma once

#include <spdlog/spdlog.h>

#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

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

  uint64_t getBestPrice() const { return bestPrice.load(); }

  uint64_t getDepth(uint8_t percent) const {
    return 0;
  }  // todo: add depth logic

 private:
  wssSubscriber wssConnection;
  const Side side;
  atomic_uint64_t bestPrice = 0;
  atomic_uint64_t quantity = 0;
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
    if (decoded.size() != sizeof(BookSide))
      throw std::runtime_error("invalid response length " +
                               std::to_string(decoded.size()) + " expected " +
                               std::to_string(sizeof(BookSide)));

    BookSide bookSide;
    memcpy(&bookSide, decoded.data(), sizeof(decltype(bookSide)));

    auto iter = BookSide::iterator(side, bookSide);

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
          bestPrice.store((uint64_t)(leafNode->key >> 64));
          quantity.store(leafNode->quantity);
          updateCallback();
          break;
        }
      }
      ++iter;
    }
  }
};
}  // namespace subscription
}  // namespace mango_v3
