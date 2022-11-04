#include "websocket.hpp"

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
session::session(net::io_context &ioc)
    : strand(net::make_strand(ioc)), resolver(net::make_strand(ioc)),
      ws(net::make_strand(ioc)) {}

/// @brief Looks up the domain name to make connection to -> calls on_resolve
/// @param host the host address
/// @param port port number
/// @param text ping message
void session::run(std::string host, std::string port) {
  // Save these for later
  //std::cout << host << " " << port << std::endl;
  this->host = host;
  tcp::resolver::query resolver_query(host, port);
  // Look up the domain name
  resolver.async_resolve(
      resolver_query,
      beast::bind_front_handler(&session::on_resolve, shared_from_this()));
}

/// @brief This function will keep running in another thread to send
/// messages to the websocket
void session::write() {
  // use strand post to get the ioc from the different thread
  // recommended here
  // https://github.com/boostorg/beast/issues/1092#issuecomment-379934484
  net::post(strand, std::bind(&session::do_write, shared_from_this()));
}

/// @brief push a function for subscription
/// @param req the request to call
void session::subscribe(std::shared_ptr<RequestContent> &req) {
  pendingSub.push(req);
  id_sub_map[req->id] = -2;  // set it to processing
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

  // Push the id for unsubscribing
  pendingUnSub.push(id);
  //std::cout<<"pending size"<<pendingUnSub.size()<<std::endl;
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
  beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

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

  // the blocking can be seen by the fact that read is called once and not
  // recursively
  //std::cout << "Reading..." << std::endl;
  // get the data from the websocket and parse it to json
  auto res = buffer.data();
  json data = json::parse(net::buffers_begin(res), net::buffers_end(res));
  //std::cout << data << std::endl;
  // denotes that this response is just after writing for subscription
  if (justSubscribed) {
    add_callback(data);
  }

  // denotes that this response is just after writing for unsubscription
  else if (justUnsubscribed) {
    remove_callback(data);
  }

  // this is a notification process it sccordingly
  else {
    call_callback(data);
  }

  // consume the buffer
  buffer.consume(bytes_transferred);

  // call read again to read continiously
  ws.async_read(
      buffer, beast::bind_front_handler(&session::on_read, shared_from_this()));
}

/// @brief function to write to the websocket
void session::do_write() {
  // if pendingSub and pendingUnSub is empty that meaans nothing to write stop
  // thread execution if justSubscribed or justUnSubscribed i.e there is one
  // async_write don't write again
  while (is_connected and
         ((pendingSub.size() == 0 and pendingUnSub.size() == 0) or
          justSubscribed or justUnsubscribed))  sleep(1)
    ;

  // if not connected then wait for 5s for connection
  // if still not connected return
  if (!is_connected) {
    auto start_time = std::chrono::steady_clock::now();
    while (!is_connected) {
      auto curr_time = std::chrono::steady_clock::now();
      int time_diff = std::chrono::duration_cast<std::chrono::seconds>(
                          curr_time - start_time)
                          .count();
      if (time_diff >= 5) {
        return;
      }
    }
  }

  // if a subscription is pending
  if (pendingSub.size() != 0) {
    // take one of the subscription from the pending subscription
    auto sub = pendingSub.front();

    // set just subscribe to true to ensure that you no other subscription is
    // made
    justSubscribed = true;
    //std::cout << "Calling Subscribe POST data ->" << std::endl;
    //std::cout << sub->get_subscription_request().dump() << std::endl;
    // get subscription request and then send it to the websocket
    //std::cout<<"pending size"<<pendingUnSub.size()<<std::endl;
    ws.async_write(
        net::buffer(sub->get_subscription_request().dump()),
        beast::bind_front_handler(&session::on_subscribe, shared_from_this()));
  }

  // if an unsubscription is pending
  if (pendingUnSub.size() != 0) {
    // take one of the unsubscription from the pending unsubscription
    int unsub_id = pendingUnSub.front();
    int unsub_num = id_sub_map[unsub_id];

    // if subscription was unsuccessfull
    if (unsub_num == -1) {
      pendingUnSub.pop();
      return do_write();
    }

    // if it is still processing
    if (unsub_num == -2) {
      return do_write();
    }

    auto unsub = callback_map[unsub_num];

    // set just subscribe to true to ensure that you no other subscription is
    // made
    justUnsubscribed = true;
    //std::cout << "Calling UnSubscribe POST data ->" << std::endl;
    //std::cout << unsub << std::endl;
    //std::cout << unsub->get_unsubscription_request(unsub_num).dump()
    //          << std::endl;
    // write it to the websocket
    ws.async_write(
        net::buffer(unsub->get_unsubscription_request(unsub_num).dump()),
        beast::bind_front_handler(&session::on_unsubscribe,
                                  shared_from_this()));
  }
}

/// @brief function to call after request of making subscription
/// @param ec error code
/// @param bytes_transferred the result of the write
void session::on_subscribe(beast::error_code ec,
                           std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    // set just subscribe to false
    justSubscribed = false;
    // return the error message
    return fail(ec, "write");
  }

  // call write again to make more subscription
  write();
}

/// @brief function to call after request of making unsubscription
/// @param ec error code
/// @param bytes_transferred the result of the write
void session::on_unsubscribe(beast::error_code ec,
                             std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    // set just subscribe to false
    justUnsubscribed = false;
    // return the error message
    return fail(ec, "write");
  }

  // call write again to make more subscription
  write();
}

/// @brief function to add callback using the respons of subscription
/// @param data the reponse of subscription from server
void session::add_callback(json &data) {
  // get the sub you have subscribed for
  auto sub = pendingSub.front();
  // pop it so you can start working with the next subscription
  pendingSub.pop();

  // set justSubscribed to false to resume writing
  justSubscribed = false;

  // if result is not a key in data then subscription was unsuccesful
  std::string result = "result";
  if (!data.contains(std::string{result})) {
    sub_id = -1;  // -1 denotes unsuccessfull
  } else {
    // update sub_id with the returned subscription id
    sub_id = data["result"];

    // add the subscription to callback map
    callback_map[sub_id] = sub;
  }
  // add the subscription to the id map
  id_sub_map[sub->id] = sub_id;
}

/// @brief function to remove callback when unsubscribing
/// @param data the data recieved from websocket
void session::remove_callback(json &data) {
  // get the sub id for the unsubscriptionstring
  int unsub_id = pendingUnSub.front();
  int unsub_num = id_sub_map[unsub_id];

  // pop it so you can start working with the next unsubscription
  pendingUnSub.pop();

  // set justunsubscribed to false to resume writing
  justUnsubscribed = false;

  // if result is not a key in data then subscription was unsuccesful
  std::string result = "result";
  if (!data.contains(std::string{result})) {
    unsub_res = 0;  // 0 denotes unsuccessfull
  } else {
    unsub_res = data["result"] ? 1 : 0;
    // if unsubscription was successful remove it from map
    if (unsub_res) {
      callback_map.erase(unsub_num);
      id_sub_map.erase(unsub_id);
    }
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
