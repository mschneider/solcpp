#pragma once

#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <thread>

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;             // from <nlohmann/json.hpp>
using Callback =
    std::function<void(const json &)>;  // callback function which takes
                                        // the json and returns nothing

using RequestIdType = unsigned long;
/// @brief Class to store the request and function given by user
struct RequestContent {
  // id will be the id for subscription while id+1 will be id for unsubscription
  RequestIdType id;
  // subscription and unsubscription method
  std::string subscribe_method;
  std::string unsubscribe_method;

  // callback to call in case of notification
  Callback cb;
  Callback on_subscribe;
  Callback on_unsubscribe;
  // params to be passed with the subscription string
  json params;
  // using promise to ensure this
  std::promise<bool> subscribed_pr;
  std::future<bool> subscription_future = subscribed_pr.get_future();
  RequestIdType ws_id;

  RequestContent() = default;

  /// @brief constructor
  RequestContent(RequestIdType id, std::string subscribe_method,
                 std::string unsubscribe_method, Callback cb, json &&params,
                 Callback on_subscribe = nullptr,
                 Callback on_unsubscribe = nullptr);

  /// @brief Get the json request to do subscription
  /// @return the json that can be used to make subscription
  json get_subscription_request() const;

  /// @brief Json data to write to unsubscribe
  /// @param subscription_id the id to subscribe for
  /// @return request json for unsubscription
  json get_unsubscription_request(RequestIdType subscription_id) const;
};

// used to create a session to read and write to websocket
class session : public std::enable_shared_from_this<session> {
 public:
  using HandshakePromisePtr = std::unique_ptr<std::promise<void>>;
  HandshakePromisePtr handshake_promise = nullptr;
  /// @brief resolver and websocket require an io context to do io operations
  /// @param ioc
  explicit session(net::io_context &ioc, int timeout_in_seconds = 30,
                   HandshakePromisePtr handshake_callback = nullptr);

  /// @brief Looks up the domain name to make connection to -> calls on_resolve
  /// @param host the host address
  /// @param port port number
  /// @param text ping message
  void run(std::string host, std::string port);

  /// @brief push a function for subscription
  /// @param req the request to call
  void subscribe(RequestContent *req);

  /// @brief push for unsubscription
  /// @param id the id to unsubscribe on
  void unsubscribe(RequestIdType id);

  /// @brief disconnect from browser
  void disconnect();

  /// @brief check if connection has been stablished
  /// @return if connection has been established
  bool connection_established();

 private:
  /// @brief log error messages
  /// @param ec the error code recieved from websocket
  /// @param what a text to log with error
  void fail(beast::error_code ec, char const *what);

  /// @brief Used to connect to the provided host -> calls on_connect
  /// @param ec error code in case of the resolve
  /// @param results the result of the resolve
  void on_resolve(beast::error_code ec, tcp::resolver::results_type results);

  /// @brief Initiates handshake for the websocket connection -> calls
  /// on_handshake
  /// @param ec the error code during connect
  /// @param
  void on_connect(beast::error_code ec,
                  tcp::resolver::results_type::endpoint_type);

  /// @brief Send the ping message once the handshake has been complete calls ->
  /// on_read
  /// @param ec error code on handshake
  void on_handshake(beast::error_code ec);

  /// @brief Deals with the message recieved from server, calls itself to keep
  /// listening to more messages
  /// @param ec error code while reading
  /// @param bytes_transferred Amount of byte recieved
  void on_read(beast::error_code ec, std::size_t bytes_transferred);

  /// @brief call the specified callback
  /// @param data the data recieved from websocket
  void call_callback(const json &data);

  /// @brief close the connection from websocket
  /// @param ec the error code
  void on_close(beast::error_code ec);

  /// @brief get callback
  /// @param request_id
  Callback get_callback(RequestIdType request_id);

  // resolves the host and port provided by user
  tcp::resolver resolver;

  // socket used by client to read and write
  websocket::stream<beast::tcp_stream> ws;

  // buffer to store data
  beast::flat_buffer buffer;

  // host
  std::string host;

  // denotes if the connection is up
  std::atomic_bool is_connected;

  // map of subscription id with callback
  std::unordered_map<RequestIdType, RequestContent *> callback_map;
  std::unordered_map<RequestIdType, RequestIdType> maps_wsid_to_id;
  std::shared_mutex mutex_for_maps;

  // connection timeout
  int connection_timeout = 30;
};