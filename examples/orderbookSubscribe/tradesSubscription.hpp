#pragma once

#include <spdlog/spdlog.h>

#include <functional>
#include <nlohmann/json.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"
#include "wssSubscriber.hpp"

namespace examples {

using json = nlohmann::json;

class tradesSubscription {
 public:
  tradesSubscription(const std::string& account) : wssConnection(account) {}

  void registerUpdateCallback(std::function<void()> callback) {
    updateCallback = callback;
  }

  void subscribe() {
    wssConnection.registerOnMessageCallback(
        std::bind(&tradesSubscription::onMessage, this, std::placeholders::_1));
    wssConnection.start();
  }

  int64_t getLastTrade() const {
    // todo: if decided to make public, have to add mtx/atomic to avoid data
    // race
    return latestTrade;
  }

 private:
  void onMessage(const json& parsedMsg) {
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
  const auto events =
      reinterpret_cast<const mango_v3::EventQueue *>(decoded.data());
  const auto seqNumDiff = events->header.seqNum - lastSeqNum;
  const auto lastSlot =
      (events->header.head + events->header.count) % mango_v3::EVENT_QUEUE_SIZE;

  // find all recent events and print them to log
  if (events->header.seqNum > lastSeqNum) {
    for (int offset = seqNumDiff; offset > 0; --offset) {
      const auto slot = (lastSlot - offset + mango_v3::EVENT_QUEUE_SIZE) %
                        mango_v3::EVENT_QUEUE_SIZE;
      const auto &event = events->items[slot];
      uint64_t timestamp = 0;
      switch (event.eventType) {
        case mango_v3::EventType::Fill: {
          const auto &fill = (mango_v3::FillEvent &)event;
          timestamp = fill.timestamp;
          const auto timeOnBook = fill.timestamp - fill.makerTimestamp;
//          spdlog::info("=====================================================");
//          spdlog::info("FILL {}", (fill.takerSide ? "sell" : "buy"));
//          spdlog::info("prc: {}", fill.price);
//          spdlog::info("qty: {}", fill.quantity);
//          spdlog::info("taker: {}", fill.taker.toBase58());
//          spdlog::info("maker: {}", fill.maker.toBase58());
//          spdlog::info("makerOrderId: {}", to_string(fill.makerOrderId));
//          spdlog::info("makerOrderClientId: {}", fill.makerClientOrderId);
//          spdlog::info("timeOnBook: {}", timeOnBook);
//          spdlog::info("makerFee: {}", fill.makerFee.toDouble());
//          spdlog::info("takerFee: {}", fill.takerFee.toDouble());
          spdlog::info("prc: {}", fill.price);
          latestTrade = fill.price;
          updateCallback();
          break;
        }
        case mango_v3::EventType::Out: {
          const auto &out = (mango_v3::OutEvent &)event;
          timestamp = out.timestamp;
          //spdlog::info(" OUT ");
          break;
        }
        case mango_v3::EventType::Liquidate: {
          const auto &liq = (mango_v3::LiquidateEvent &)event;
          timestamp = liq.timestamp;
//          spdlog::info("LIQ prc: {} qty: {}", liq.price.toDouble(),
//                       liq.quantity);
          break;
        }
      }

      const uint64_t lag =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count() -
          timestamp * 1000;
      //spdlog::info("lag: {} ms", lag);
    }
  }

  lastSeqNum = events->header.seqNum;
  }

  uint64_t lastSeqNum = INT_MAX;
  int64_t latestTrade = 0;
  wssSubscriber wssConnection;
  std::function<void()> updateCallback;
};
}  // namespace examples
