#include <spdlog/spdlog.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "int128.hpp"
#include "mango_v3.hpp"

typedef websocketpp::client<websocketpp::config::asio_tls_client> ws_client;
typedef websocketpp::config::asio_client::message_type::ptr ws_message_ptr;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;

using json = nlohmann::json;

static context_ptr on_tls_init() {
  // establishes a SSL connection
  context_ptr ctx = std::make_shared<boost::asio::ssl::context>(
      boost::asio::ssl::context::sslv23);

  try {
    ctx->set_options(boost::asio::ssl::context::default_workarounds |
                     boost::asio::ssl::context::no_sslv2 |
                     boost::asio::ssl::context::no_sslv3 |
                     boost::asio::ssl::context::single_dh_use);
  } catch (std::exception &e) {
    spdlog::error("Error in context pointer: {}", e.what());
  }
  return ctx;
}

void on_open(ws_client *c, websocketpp::connection_hdl hdl) {
  // subscribe to btc-perp eventQ
  const std::string account = "7t5Me8RieYKsFpfLEV8jnpqcqswNpyWD95ZqgUXuLV8Z";
  websocketpp::lib::error_code ec;

  c->send(hdl,
          solana::rpc::subscription::accountSubscribeRequest(account).dump(),
          websocketpp::frame::opcode::value::text, ec);
  if (ec) {
    spdlog::error("subscribe failed because: {}", ec.message());
  } else {
    spdlog::info("subscribed to {}", account);
  }
}

uint64_t lastSeqNum = INT_MAX;

void on_message(ws_client *c, websocketpp::connection_hdl hdl,
                ws_message_ptr msg) {
  const json parsedMsg = json::parse(msg->get_payload());

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
          spdlog::info("=====================================================");
          spdlog::info("FILL {}", (fill.takerSide ? "sell" : "buy"));
          spdlog::info("prc: {}", fill.price);
          spdlog::info("qty: {}", fill.quantity);
          spdlog::info("taker: {}", fill.taker.toBase58());
          spdlog::info("maker: {}", fill.maker.toBase58());
          spdlog::info("makerOrderId: {}", to_string(fill.makerOrderId));
          spdlog::info("makerOrderClientId: {}", fill.makerClientOrderId);
          spdlog::info("timeOnBook: {}", timeOnBook);
          spdlog::info("makerFee: {}", fill.makerFee.to_double());
          spdlog::info("takerFee: {}", fill.takerFee.to_double());
          break;
        }
        case mango_v3::EventType::Out: {
          const auto &out = (mango_v3::OutEvent &)event;
          timestamp = out.timestamp;
          spdlog::info(" OUT ");
          break;
        }
        case mango_v3::EventType::Liquidate: {
          const auto &liq = (mango_v3::LiquidateEvent &)event;
          timestamp = liq.timestamp;
          spdlog::info("LIQ prc: {} qty: {}", liq.price.to_double(),
                       liq.quantity);
          break;
        }
      }

      const uint64_t lag =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count() -
          timestamp * 1000;
      spdlog::info("lag: {} ms", lag);
    }
  }

  lastSeqNum = events->header.seqNum;
}

int main() {
  try {
    ws_client c;

    c.set_access_channels(websocketpp::log::alevel::all);

    // Initialize ASIO
    c.init_asio();
    c.set_tls_init_handler(websocketpp::lib::bind(&on_tls_init));

    // Register message handlers
    c.set_open_handler(websocketpp::lib::bind(
        &on_open, &c, websocketpp::lib::placeholders::_1));
    c.set_message_handler(websocketpp::lib::bind(
        &on_message, &c, websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2));

    websocketpp::lib::error_code ec;
    ws_client::connection_ptr con = c.get_connection(
        "wss://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88", ec);
    if (ec) {
      spdlog::error("could not create connection because: {}", ec.message());
      return 0;
    }

    // Note that connect here only requests a connection. No network messages
    // are exchanged until the event loop starts running in the next line.
    c.connect(con);

    // Start the ASIO io_service run loop
    // this will cause a single connection to be made to the server. c.run()
    // will exit when this connection is closed.
    c.run();
  } catch (websocketpp::exception const &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
