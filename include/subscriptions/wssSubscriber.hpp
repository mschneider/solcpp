#pragma once

#include <spdlog/spdlog.h>

#include <functional>
#include <thread>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

typedef websocketpp::client<websocketpp::config::asio_tls_client> ws_client;
typedef websocketpp::config::asio_client::message_type::ptr ws_message_ptr;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;

namespace mango_v3 {
namespace subscription {

using json = nlohmann::json;

class wssSubscriber {
 public:
  wssSubscriber(const std::string& account) : account(account) {}

  ~wssSubscriber() { runThread.join(); }

  void registerOnMessageCallback(
      std::function<void(const json& message)> callback) {
    onMessageCb = callback;
  }

  void start() {
    runThread = std::thread(&wssSubscriber::subscriptionThread, this);
  }

 private:
  void subscriptionThread() {
    try {
      c.set_access_channels(websocketpp::log::alevel::all);
      c.init_asio();
      c.set_tls_init_handler(
          websocketpp::lib::bind(&wssSubscriber::on_tls_init, this));

      c.set_open_handler(websocketpp::lib::bind(
          &wssSubscriber::on_open, this, websocketpp::lib::placeholders::_1));
      c.set_message_handler(websocketpp::lib::bind(
          &wssSubscriber::on_message, this, websocketpp::lib::placeholders::_1,
          websocketpp::lib::placeholders::_2));

      websocketpp::lib::error_code ec;
      ws_client::connection_ptr con = c.get_connection(
          "wss://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88", ec);
      if (ec) {
        spdlog::error("could not create connection because: {}", ec.message());
      }
      c.connect(con);
      c.run();
    } catch (websocketpp::exception const& e) {
      std::cout << e.what() << std::endl;
    }
  }

  context_ptr on_tls_init() {
    context_ptr ctx = std::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::sslv23);

    try {
      ctx->set_options(boost::asio::ssl::context::default_workarounds |
                       boost::asio::ssl::context::no_sslv2 |
                       boost::asio::ssl::context::no_sslv3 |
                       boost::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
      spdlog::error("Error in context pointer: {}", e.what());
    }
    return ctx;
  }

  void on_open(websocketpp::connection_hdl hdl) {
    websocketpp::lib::error_code ec;

    c.send(hdl,
           solana::rpc::subscription::accountSubscribeRequest(account).dump(),
           websocketpp::frame::opcode::value::text, ec);
    if (ec) {
      spdlog::error("subscribe failed because: {}", ec.message());
    } else {
      spdlog::info("subscribed to account {}", account);
    }
  }

  void on_message(websocketpp::connection_hdl hdl, ws_message_ptr msg) {
    const json parsedMsg = json::parse(msg->get_payload());
    onMessageCb(parsedMsg);
  }

  ws_client c;
  const std::string account;
  std::thread runThread;
  std::function<void(const json&)> onMessageCb;
};
}  // namespace subscription
}  // namespace mango_v3
