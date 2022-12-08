#include "websocket.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;  // from <nlohmann/json.hpp>

/// @brief constructor
RequestContent::RequestContent(RequestIdType id, std::string subscribe_method,
                               std::string unsubscribe_method, Callback cb,
                               json &&params, Callback on_subscribe,
                               Callback on_unsubscribe)
    : id(id), subscribe_method(std::move(subscribe_method)),
      unsubscribe_method(std::move(unsubscribe_method)), cb(cb),
      params(std::move(params)), on_subscribe(on_subscribe),
      on_unsubscribe(on_unsubscribe) {}

/// @brief Get the json request to do subscription
/// @return the json that can be used to make subscription
json RequestContent::get_subscription_request() const {
  json req = {{"jsonrpc", "2.0"},
              {"id", this->id},
              {"method", this->subscribe_method},
              {"params", this->params}};
  return req;
}

/// @brief Json data to write to unsubscribe
/// @param subscription_id the id to subscribe for
/// @return request json for unsubscription
json RequestContent::get_unsubscription_request(
    RequestIdType subscription_id) const {
  json params = {subscription_id};
  json req = {{"jsonrpc", "2.0"},
              {"id", this->id + 1},
              {"method", this->unsubscribe_method},
              {"params", params}};
  return req;
}

/// @brief resolver and websocket require an io context to do io operations
/// @param ioc
session::session(net::io_context &ioc, int timeout_in_seconds,
                 session::HandshakePromisePtr handshake_promise)
    : resolver(net::make_strand(ioc)), ws(net::make_strand(ioc)),
      connection_timeout(timeout_in_seconds),
      handshake_promise(std::move(handshake_promise)) {
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
void session::subscribe(RequestContent *req) {
  // context for unique_lock
  {
    std::unique_lock lk(mutex_for_maps);
    callback_map[req->id] = req;
  }
  // get subscription request and then send it to the websocket
  ws.write(net::buffer(req->get_subscription_request().dump()));
}

/// @brief push for unsubscription
/// @param id the id to unsubscribe on
void session::unsubscribe(RequestIdType id) {
  // context for shared lock
  std::string unsubsciption_request = "";
  std::unordered_map<RequestIdType, RequestContent *>::iterator ite;
  {
    std::shared_lock lk(mutex_for_maps);

    ite = callback_map.find(id);
  }
  if (ite == callback_map.end()) return;

  // wait for subscription to happen
  bool subscription_successful = ite->second->subscription_future.get();

  // if subscription wasn't successful then just remove the id from callback
  if (!subscription_successful) {
    {
      std::unique_lock lk(mutex_for_maps);
      const auto ite = callback_map.find(id);
      if (ite != callback_map.end()) {
        callback_map.erase(ite);
        delete ite->second;
      }
    }
    return;
  }

  unsubsciption_request =
      ite->second->get_unsubscription_request(ite->second->ws_id).dump();
  maps_wsid_to_id.erase(ite->second->ws_id);
  if (!unsubsciption_request.empty()) {
    // write it to the websocket
    ws.write(net::buffer(unsubsciption_request));
  }
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
bool session::connection_established() { return is_connected.load(); }

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

  if (handshake_promise) handshake_promise->set_value();

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
  static const char *result = "result";
  static const char *error = "error";
  try {
    if (data.contains(std::string{result})) {
      RequestIdType id = data["id"];
      // if the result field is boolean than it's an unsubscription request
      // could be ignored
      if (data[result].is_boolean()) {
        // usually this means that we are unsubscribing
        Callback on_unsubscribe = nullptr;
        id--;
        // context for lock
        {
          std::unique_lock lk(mutex_for_maps);
          const auto ite = callback_map.find(id);
          if (ite != callback_map.end()) {
            on_unsubscribe = ite->second->on_unsubscribe;
            callback_map.erase(ite);
            delete ite->second;
          }
        }
        if (on_unsubscribe) {
          on_unsubscribe(data);
        }
      } else {
        // usually this means that our subscription request has been successfull
        // context for lock
        Callback on_subscribe = nullptr;
        {
          std::unique_lock lk(mutex_for_maps);

          const auto ite = callback_map.find(id);
          if (ite != callback_map.end()) {
            on_subscribe = ite->second->on_subscribe;
            ite->second->subscribed_pr.set_value(true);
            ite->second->ws_id = data[result];
            maps_wsid_to_id[ite->second->ws_id] = id;
          }
        }
        if (on_subscribe) {
          on_subscribe(data);
        }
      }
    }
    // In case of erro
    else if (data.contains(std::string{error})) {
      RequestIdType id = data["id"];
      json er_mess = data["error"];
      // if id is even then error in subscribing else error in unsubscribing
      if (id % 2 == 0) {
        std::cout << "Some error happened while subscribing" << std::endl;
        {
          std::unique_lock lk(mutex_for_maps);

          const auto ite = callback_map.find(id);
          if (ite != callback_map.end()) {
            ite->second->subscribed_pr.set_value(false);
          }
        }
      } else {
        std::cout << "Some error happened while unsubscribing" << std::endl;
      }
      std::cout << er_mess << std::endl;
    }
    // it's a notification process it sccordingly
    else {
      call_callback(data);
    }
  } catch (...) {
    // catch any exception so that we still continue reading
    std::cerr << "An exception was caught while dispatching the error";
  }

  // consume the buffer
  buffer.consume(bytes_transferred);

  // call read again to read continiously
  ws.async_read(
      buffer, beast::bind_front_handler(&session::on_read, shared_from_this()));
}

/// @brief close the connection from websocket
/// @param ec the error code
Callback session::get_callback(RequestIdType request_id) {
  std::shared_lock lk(mutex_for_maps);
  auto id_ite = maps_wsid_to_id.find(request_id);
  if (id_ite == maps_wsid_to_id.end()) {
    // we have already unsubscribed so no need to call the callback
    return nullptr;
  }
  RequestIdType id = id_ite->second;
  auto sub_ite = callback_map.find(id);
  if (sub_ite == callback_map.end()) {
    std::cerr << "subscription found but request not found " << request_id
              << std::endl;
    return nullptr;
  }
  return sub_ite->second->cb;
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
}

/// @brief close the connection from websocket
/// @param ec the error code
void session::on_close(beast::error_code ec) {
  if (ec) return fail(ec, "close");

  // If we get here then the connection is closed gracefully
  return;
}