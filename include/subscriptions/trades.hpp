#pragma once

#include <spdlog/spdlog.h>

#include <functional>
#include <nlohmann/json.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"
#include "wssSubscriber.hpp"

namespace mango_v3 {
namespace subscription {

using json = nlohmann::json;

class trades {
 public:
  trades(const std::string &account) : wssConnection(account) {}

  void registerUpdateCallback(std::function<void()> callback) {
    notifyCb = callback;
  }

  void subscribe() {
    wssConnection.registerOnMessageCallback(
        std::bind(&trades::onMessage, this, std::placeholders::_1));
    wssConnection.start();
  }

  int64_t getLastTrade() const {
    const std::scoped_lock lock(latestTradeMtx);
    return latestTrade;
  }

 private:
  void onMessage(const json &parsedMsg) {
    // ignore subscription confirmation
    const auto itResult = parsedMsg.find("result");
    if (itResult != parsedMsg.end()) {
      spdlog::info("on_result {}", parsedMsg.dump());
      return;
    }

    // all other messages are event queue updates
    const std::string method = parsedMsg["method"];
    const int subscription = parsedMsg["params"]["subscription"];
    const int slot = parsedMsg["params"]["result"]["context"]["slot"];
    const std::string data = parsedMsg["params"]["result"]["value"]["data"][0];

    const auto decoded = solana::b64decode(data);
    const auto events = reinterpret_cast<const EventQueue *>(decoded.data());
    const auto seqNumDiff = events->header.seqNum - lastSeqNum;
    const auto lastSlot =
        (events->header.head + events->header.count) % EVENT_QUEUE_SIZE;

    bool gotLatest = false;
    if (events->header.seqNum > lastSeqNum) {
      for (int offset = seqNumDiff; offset > 0; --offset) {
        const auto slot =
            (lastSlot - offset + EVENT_QUEUE_SIZE) % EVENT_QUEUE_SIZE;
        const auto &event = events->items[slot];

        if (event.eventType == EventType::Fill) {
          const auto &fill = (FillEvent &)event;
          const std::scoped_lock lock(latestTradeMtx);
          latestTrade = fill.price;
          gotLatest = true;
        }
        // no break; let's iterate to the last fill to get the latest fill order
      }
    }

    if (gotLatest) {
      notifyCb();
    }
    lastSeqNum = events->header.seqNum;
  }

  uint64_t lastSeqNum = INT_MAX;
  // todo:macos latomic not found issue, otherwise replace mtx with std::atomic
  mutable std::mutex latestTradeMtx;
  uint64_t latestTrade = 0;
  wssSubscriber wssConnection;
  std::function<void()> notifyCb;
};
}  // namespace subscription
}  // namespace mango_v3
