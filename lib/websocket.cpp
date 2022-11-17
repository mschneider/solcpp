#include "websocket.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;  // from <nlohmann/json.hpp>

/// @brief constructor
RequestContent::RequestContent(RequestIdType id, std::string subscribe_method,
                               std::string unsubscribe_method, Callback cb,
                               json &&params)
    : id(id), subscribe_method(std::move(subscribe_method)),
      unsubscribe_method(std::move(unsubscribe_method)), cb(cb), params(std::move(params)) {}

/// @brief Get the json request to do subscription
/// @return the json that can be used to make subscription
json RequestContent::get_subscription_request() const {
  json req = {{"jsonrpc", "2.0"},
              {"id", id},
              {"method", subscribe_method},
              {"params", params}};
  return req;
}

/// @brief Json data to write to unsubscribe
/// @param subscription_id the id to subscribe for
/// @return request json for unsubscription
json RequestContent::get_unsubscription_request(RequestIdType subscription_id) const {
  json params = {subscription_id};
  json req = {{"jsonrpc", "2.0"},
              {"id", id + 1},
              {"method", unsubscribe_method},
              {"params", params}};
  return req;
}

/// @brief resolver and websocket require an io context to do io operations
/// @param ioc
session::session(net::io_context &ioc, int timeout_in_seconds)
    : resolver(net::make_strand(ioc)), ws(net::make_strand(ioc)),
      connection_timeout(timeout_in_seconds) {
        is_connected.store(false);
      }

/// @brief Looks up the domain name to make connection to -> calls on_resolve
/// @param host the host address
/// @param port port number
/// @param text ping message
void session::run(std::string host, std::string port) {
  // Save these for later
  this->host = host;
  tcp::resolver::query resolver_query(host, port);
  // Look up the domain name
  resolver.async_resolve(
      resolver_query,
      beast::bind_front_handler(&session::on_resolve, shared_from_this()));
}

/// @brief push a function for subscription
/// @param req the request to call
void session::subscribe( const RequestContent &req) {
  std::unique_lock lk (mutex_for_maps);
  callback_map[req.id] = req;
  // get subscription request and then send it to the websocket
  ws.write(net::buffer(req.get_subscription_request().dump()));
}

/// @brief push for unsubscription
/// @param id the id to unsubscribe on
void session::unsubscribe(RequestIdType id) {
  auto ite = callback_map.find(id);
  if (ite == callback_map.end()) return;

  std::unique_lock lk (mutex_for_maps);
  const RequestContent context = ite->second;
  callback_map.erase(ite);
  // write it to the websocket
  ws.write(net::buffer(context.get_unsubscription_request(id).dump()));
}

/// @brief disconnect from browser
void session::disconnect() {
  // set is connected to false to close the read and write thread
  is_connected.store(false);

  // Close the WebSocket connection
  ws.async_close(
      websocket::close_code::normal,
      beast::bind_front_handler(&session::on_close, shared_from_this()));
}

/// @brief check if connection has been stablished
/// @return if connection has been established
bool session::connection_established() {
  return is_connected.load();
}

/// @brief log error messages
/// @param ec the error code recieved from websocket
/// @param what a text to log with error
void session::fail(beast::error_code ec, char const *what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

/// @brief Used to connect to the provided host -> calls on_connect
/// @param ec error code in case of the resolve
/// @param results the result of the resolve
void session::on_resolve(beast::error_code ec,
                         tcp::resolver::results_type results) {
  if (ec) return fail(ec, "resolve");

  // Set the timeout for the operation
  beast::get_lowest_layer(ws).expires_after(
      std::chrono::seconds(connection_timeout));

  // Make the connection on the IP address we get from a lookup
  beast::get_lowest_layer(ws).async_connect(
      results,
      beast::bind_front_handler(&session::on_connect, shared_from_this()));
}

/// @brief Initiates handshake for the websocket connection -> calls
/// on_handshake
/// @param ec the error code during connect
/// @param
void session::on_connect(beast::error_code ec,
                         tcp::resolver::results_type::endpoint_type) {
  if (ec) return fail(ec, "connect");

  // Turn off the timeout on the tcp_stream, because
  // the websocket stream has its own timeout system.
  beast::get_lowest_layer(ws).expires_never();

  // Set suggested timeout settings for the websocket
  ws.set_option(
      websocket::stream_base::timeout::suggested(beast::role_type::client));

  // Set a decorator to change the User-Agent of the handshake
  ws.set_option(
      websocket::stream_base::decorator([](websocket::request_type &req) {
        req.set(http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-async");
      }));

  // Perform the websocket handshake
  ws.async_handshake(
      host, "/",
      beast::bind_front_handler(&session::on_handshake, shared_from_this()));
}

/// @brief Send the ping message once the handshake has been complete calls ->
/// on_read
/// @param ec error code on handshake
void session::on_handshake(beast::error_code ec) {
  if (ec) return fail(ec, "handshake");

  // If you reach here connection is up set is_connected to true
  is_connected.store(true);

  // Start listening to the socket for messages
  ws.async_read(
      buffer, beast::bind_front_handler(&session::on_read, shared_from_this()));
}

/// @brief Deals with the message recieved from server, calls itself to keep
/// listening to more messages
/// @param ec error code while reading
/// @param bytes_transferred Amount of byte recieved
void session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);
  // report error on read
  if (ec) {
    return fail(ec, "read");
  }

  // id socket has been disconnected then return
  if (!is_connected.load()) {
    return;
  }

  // get the data from the websocket and parse it to json
  auto res = buffer.data();
  json data = json::parse(net::buffers_begin(res), net::buffers_end(res));

  // if data contains field result then it's either subscription or
  // unsubscription response
  std::string result = "result";
  if (data.contains(std::string{result})) {
    // if the result field is boolean than it's an unsubscription request could be ignored
    return;
  }
  // it's a notification process it sccordingly
  else {
    call_callback(data);
  }

  // consume the buffer
  buffer.consume(bytes_transferred);

  // call read again to read continiously
  ws.async_read(
      buffer, beast::bind_front_handler(&session::on_read, shared_from_this()));
}

/// @brief close the connection from websocket
/// @param ec the error code
Callback session::get_callback(RequestIdType request_id)
{
  std::shared_lock lk(mutex_for_maps);
  if (callback_map.find(request_id) != callback_map.end()) {
    return callback_map[request_id].cb;
  }
  return nullptr;
}

/// @brief call the specified callback
/// @param data the data recieved from websocket
void session::call_callback(const json &data) {
  // if params is not a key then return
  std::string par = "params";
  if (!data.contains(std::string{par})) {
    return;
  }

  // if subscription is not a key in data["params"] then it's not relevant
  std::string subscription = "subscription";
  if (!data["params"].contains(std::string{subscription})) {
    return;
  }
  // call the specified callback related to the subscription id
  RequestIdType returned_sub = data["params"]["subscription"];
  Callback cb = get_callback(returned_sub);
  if (cb != nullptr) {
    cb(data);
  }
  else {
    std::cerr << "recieved message for " << returned_sub << " probably already unsubscribed";
  }
}

/// @brief close the connection from websocket
/// @param ec the error code
void session::on_close(beast::error_code ec) {
  if (ec) return fail(ec, "close");

  // If we get here then the connection is closed gracefully
  return;
}
