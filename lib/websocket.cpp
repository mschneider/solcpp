#include "websocket.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;  // from <nlohmann/json.hpp>

/// @brief constructor
RequestContent::RequestContent(int id, std::string subscribe_method,
                               std::string unsubscribe_method, Callback cb,
                               json params)
    : id(id), subscribe_method(subscribe_method),
      unsubscribe_method(unsubscribe_method), cb(cb), params(params) {}

/// @brief Get the json request to do subscription
/// @return the json that can be used to make subscription
json RequestContent::get_subscription_request() {
  json req = {{"jsonrpc", "2.0"},
              {"id", id},
              {"method", subscribe_method},
              {"params", params}};
  return req;
}

/// @brief Json data to write to unsubscribe
/// @param subscription_id the id to subscribe for
/// @return request json for unsubscription
json RequestContent::get_unsubscription_request(int subscription_id) {
  json params = {subscription_id};
  json req = {{"jsonrpc", "2.0"},
              {"id", id + 1},
              {"method", unsubscribe_method},
              {"params", params}};
  return req;
}

/// @brief resolver and websocket require an io context to do io operations
/// @param ioc
session::session(net::io_context &ioc, int timeout)
    : resolver(net::make_strand(ioc)), ws(net::make_strand(ioc)),
      connection_timeout(timeout) {}

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
void session::subscribe(std::shared_ptr<RequestContent> &req) {
  // store the request in latest sub
  latest_sub = req;
  id_sub_map[req->id] = -1;  // set it to processing

  // get subscription request and then send it to the websocket
  ws.write(net::buffer(req->get_subscription_request().dump()));
}

/// @brief push for unsubscription
/// @param id the id to unsubscribe on
void session::unsubscribe(int id) {
  // check if the id is part of id_sub_map
  if (id_sub_map.find(id) == id_sub_map.end()) return;

  // if the result of subscription was failure just remove it
  // as no subscription was done anyways
  if (id_sub_map[id] == -1) {
    id_sub_map.erase(id);
    return;
  }

  // store the id
  latest_unsub = id;

  // get the subscription number to unsubscribe for
  int unsub_num = id_sub_map[id];

  // get the RequestContent to unsubscribe for
  auto unsub = callback_map[unsub_num];

  // write it to the websocket
  ws.write(net::buffer(unsub->get_unsubscription_request(unsub_num).dump()));
}

/// @brief disconnect from browser
void session::disconnect() {
  // set is connected to false to close the read and write thread
  is_connected = false;

  // Close the WebSocket connection
  ws.async_close(
      websocket::close_code::normal,
      beast::bind_front_handler(&session::on_close, shared_from_this()));
}

/// @brief check if connection has been stablished
/// @return if connection has been established
bool session::connection_established() {
  // wait for 1 second
  sleep(1);
  return is_connected;
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
  is_connected = true;

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
  if (!is_connected) {
    return;
  }

  // get the data from the websocket and parse it to json
  auto res = buffer.data();
  json data = json::parse(net::buffers_begin(res), net::buffers_end(res));

  // if data contains field result then it's either subscription or
  // unsubscription response
  std::string result = "result";
  if (data.contains(std::string{result})) {
    // if the result field is boolean than it's an unsubscription request
    if (data["result"].is_boolean()) {
      remove_callback(data);
    } else {
      add_callback(data);
    }
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

/// @brief function to add callback using the respons of subscription
/// @param data the reponse of subscription from server
void session::add_callback(json &data) {
  if (latest_sub == NULL) {
    return;
  }

  // update sub_id with the returned subscription id
  int sub_id = data["result"];

  // add the subscription to callback map
  callback_map[sub_id] = latest_sub;

  // add the subscription to the id map
  id_sub_map[latest_sub->id] = sub_id;
}

/// @brief function to remove callback when unsubscribing
/// @param data the data recieved from websocket
void session::remove_callback(json &data) {
  // get id related with the latest unsub
  int unsub_num = id_sub_map[latest_unsub];

  // if unsubscription was successful remove it from map
  if (data["result"]) {
    callback_map.erase(unsub_num);
    id_sub_map.erase(latest_unsub);
  }
}

/// @brief call the specified callback
/// @param data the data recieved from websocket
void session::call_callback(json &data) {
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
  int returned_sub = data["params"]["subscription"];
  if (callback_map.find(returned_sub) != callback_map.end()) {
    callback_map[returned_sub]->cb(data);
  }
}

/// @brief close the connection from websocket
/// @param ec the error code
void session::on_close(beast::error_code ec) {
  if (ec) return fail(ec, "close");

  // If we get here then the connection is closed gracefully
  return;
}
