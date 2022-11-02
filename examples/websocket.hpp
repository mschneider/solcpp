#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <queue>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;            // from <nlohmann/json.hpp>

typedef std::function<void(json &)>
    Callback; // callback function which takes the json and returns nothing

/// @brief Class to store the request and function given by user
struct RequestContent
{
  // id will be the id for subscription while id+1 will be id for unsubscription
  int id_;
  // subscription and unsubscription method
  std::string subscribe_method_;
  std::string unsubscribe_method_;

  // callback to call in case of notification
  Callback cb_;
  // params to be passed with the subscription string
  json params_;

  /// @brief constructor
  RequestContent(int id, std::string subscribe_method,
                 std::string unsubscribe_method, Callback cb, json params)
      : id_(id), subscribe_method_(subscribe_method),
        unsubscribe_method_(unsubscribe_method), cb_(cb), params_(params) {}

  /// @brief Get the json request to do subscription
  /// @return the json that can be used to make subscription
  json get_subscription_request()
  {
    json req = {{"jsonrpc", "2.0"},
                {"id", id_},
                {"method", subscribe_method_},
                {"params", params_}};
    return req;
  }

  /// @brief Json data to write to unsubscribe
  /// @param subscription_id the id to subscribe for
  /// @return request json for unsubscription
  json get_unsubscription_request(int subscription_id)
  {
    json params = {subscription_id};
    json req = {{"jsonrpc", "2.0"},
                {"id", id_ + 1},
                {"method", unsubscribe_method_},
                {"params", params}};
    return req;
  }
};

// used to create a session to read and write to websocket
class session : public std::enable_shared_from_this<session>
{
public:
  /// @brief resolver and websocket require an io context to do io operations
  /// @param ioc
  explicit session(net::io_context &ioc)
      : strand_(net::make_strand(ioc)), resolver_(net::make_strand(ioc)),
        ws_(net::make_strand(ioc)) {}

  /// @brief Looks up the domain name to make connection to -> calls on_resolve
  /// @param host the host address
  /// @param port port number
  /// @param text ping message
  void run(std::string host, std::string port)
  {
    // Save these for later
    std::cout<<host<<" "<<port<<std::endl;
    host_ = host;
    tcp::resolver::query resolver_query(host,
                                        port);
    // Look up the domain name
    resolver_.async_resolve(
        resolver_query,
        beast::bind_front_handler(&session::on_resolve, shared_from_this()));
  }

  /// @brief This function will keep running in another thread to send
  /// messages to the websocket
  void write()
  {
    // use strand post to get the ioc from the different thread
    // recommended here
    // https://github.com/boostorg/beast/issues/1092#issuecomment-379934484
    net::post(strand_, std::bind(&session::do_write, shared_from_this()));
  }

  /// @brief push a function for subscription
  /// @param req the request to call
  void subscribe(std::shared_ptr<RequestContent> &req)
  {
    pendingSub.push(req);
    id_sub_map[req->id_] = -2; // set it to processing
  }

  /// @brief push for unsubscription
  /// @param id the id to unsubscribe on
  void unsubscribe(int id)
  {
    // check if the id is part of id_sub_map
    if (id_sub_map.find(id) == id_sub_map.end())
      return;

    // if the result of subscription was failure just remove it
    // as no subscription was done anyways
    if (id_sub_map[id] == -1)
    {
      id_sub_map.erase(id);
    }

    // this will work for -2 (processing) as well since we are subscribing first
    // then unsubscribing
    pendingUnSub.push(id_sub_map[id]);
  }

  /// @brief disconnect from browser
  void disconnect()
  {
    // set is connected to false to close the read and write thread
    is_connected = false;

    // Close the WebSocket connection
    ws_.async_close(
        websocket::close_code::normal,
        beast::bind_front_handler(&session::on_close, shared_from_this()));
  }

private:
  /// @brief log error messages
  /// @param ec the error code recieved from websocket
  /// @param what a text to log with error
  void fail(beast::error_code ec, char const *what)
  {
    std::cerr << what << ": " << ec.message() << "\n";
  }
  /// @brief Used to connect to the provided host -> calls on_connect
  /// @param ec error code in case of the resolve
  /// @param results the result of the resolve
  void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
  {
    if (ec)
      return fail(ec, "resolve");

    // Set the timeout for the operation
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&session::on_connect, shared_from_this()));
  }

  /// @brief Initiates handshake for the websocket connection -> calls
  /// on_handshake
  /// @param ec the error code during connect
  /// @param
  void on_connect(beast::error_code ec,
                  tcp::resolver::results_type::endpoint_type)
  {
    if (ec)
      return fail(ec, "connect");

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(ws_).expires_never();

    // Set suggested timeout settings for the websocket
    ws_.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Set a decorator to change the User-Agent of the handshake
    ws_.set_option(
        websocket::stream_base::decorator([](websocket::request_type &req)
                                          { req.set(http::field::user_agent,
                                                    std::string(BOOST_BEAST_VERSION_STRING) +
                                                        " websocket-client-async"); }));

    // Perform the websocket handshake
    ws_.async_handshake(
        host_, "/",
        beast::bind_front_handler(&session::on_handshake, shared_from_this()));
  }

  /// @brief Send the ping message once the handshake has been complete calls ->
  /// on_read
  /// @param ec error code on handshake
  void on_handshake(beast::error_code ec)
  {
    if (ec)
      return fail(ec, "handshake");

    // If you reach here connection is up set is_connected to true
    is_connected = true;

    // Start listening to the socket for messages
    ws_.async_read(buffer_, beast::bind_front_handler(&session::on_read,
                                                      shared_from_this()));
  }

  /// @brief Deals with the message recieved from server, calls itself to keep
  /// listening to more messages
  /// @param ec error code while reading
  /// @param bytes_transferred Amount of byte recieved
  void on_read(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);
    // report error on read
    if (ec)
    {
      return fail(ec, "read");
    }

    // id socket has been disconnected then return
    if (!is_connected)
    {
      return;
    }

    // the blocking can be seen by the fact that read is called once and not recursively
    std::cout<<"Reading..."<<std::endl;
    // get the data from the websocket and parse it to json
    auto res = buffer_.data();
    json data = json::parse(net::buffers_begin(res), net::buffers_end(res));
    std::cout << data << std::endl;
    // denotes that this response is just after writing for subscription
    if (justSubscribed)
    {
      add_callback(data);
    }

    // denotes that this response is just after writing for unsubscription
    else if (justUnsubscribed)
    {
      remove_callback(data);
    }

    // this is a notification process it sccordingly
    else
    {
      call_callback(data);
    }

    // consume the buffer
    buffer_.consume(bytes_transferred);

    // call read again to read continiously
    ws_.async_read(buffer_, beast::bind_front_handler(&session::on_read,
                                                      shared_from_this()));
  }

  /// @brief function to write to the websocket
  void do_write()
  {
    // if pendingSub and pendingUnSub is empty that meaans nothing to write stop
    // thread execution if justSubscribed or justUnSubscribed i.e there is one
    // async_write don't write again
    while (is_connected and
           ((pendingSub.size() == 0 and pendingUnSub.size() == 0) or
            justSubscribed or justUnsubscribed))
      ;

    // if not connected then wait for 5s for connection
    // if still not connected return
    if (!is_connected)
    {
      auto start_time = std::chrono::steady_clock::now();
      while (!is_connected)
      {
        auto curr_time = std::chrono::steady_clock::now();
        int time_diff = std::chrono::duration_cast<std::chrono::seconds>(
                            curr_time - start_time)
                            .count();
        if (time_diff >= 5)
        {
          return;
        }
      }
    }

    // if a subscription is pending
    if (pendingSub.size() != 0)
    {
      // take one of the subscription from the pending subscription
      auto sub = pendingSub.front();

      // set just subscribe to true to ensure that you no other subscription is
      // made
      justSubscribed = true;
      std::cout << "Calling Subscribe POST data ->" << std::endl;
      std::cout << sub->get_subscription_request().dump() << std::endl;
      // get subscription request and then send it to the websocket
      ws_.async_write(net::buffer(sub->get_subscription_request().dump()),
                      beast::bind_front_handler(&session::on_subscribe,
                                                shared_from_this()));
    }

    // if an unsubscription is pending
    if (pendingUnSub.size() != 0)
    {
      // take one of the unsubscription from the pending unsubscription
      int unsub_num = pendingUnSub.front();
      auto unsub = callback_map[unsub_num];

      // set just subscribe to true to ensure that you no other subscription is
      // made
      justUnsubscribed = true;
      std::cout << "Calling UnSubscribe POST data ->" << std::endl;
      std::cout << unsub->get_unsubscription_request(unsub_num).dump()
                << std::endl;
      // write it to the websocket
      ws_.async_write(
          net::buffer(unsub->get_unsubscription_request(unsub_num).dump()),
          beast::bind_front_handler(&session::on_unsubscribe,
                                    shared_from_this()));
    }
  }

  /// @brief function to call after request of making subscription
  /// @param ec error code
  /// @param bytes_transferred the result of the write
  void on_subscribe(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
    {
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
  void on_unsubscribe(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
    {
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
  void add_callback(json &data)
  {
    // get the sub you have subscribed for
    auto sub = pendingSub.front();
    // pop it so you can start working with the next subscription
    pendingSub.pop();

    // set justSubscribed to false to resume writing
    justSubscribed = false;

    // if result is not a key in data then subscription was unsuccesful
    std::string result = "result";
    if (!data.contains(std::string{result}))
    {
      sub_id = -1; // -1 denotes unsuccessfull
    }
    else
    {
      // update sub_id with the returned subscription id
      sub_id = data["result"];

      // add the subscription to callback map
      callback_map[sub_id] = sub;
    }
    // add the subscription to the id map
    id_sub_map[sub->id_] = sub_id;
  }

  /// @brief function to remove callback when unsubscribing
  /// @param data the data recieved from websocket
  void remove_callback(json &data)
  {
    // get the sub id for the unsubscriptionstring
    auto unsub_id = pendingUnSub.front();
    // pop it so you can start working with the next unsubscription
    pendingUnSub.pop();

    // set justunsubscribed to false to resume writing
    justUnsubscribed = false;

    // if result is not a key in data then subscription was unsuccesful
    std::string result = "result";
    if (!data.contains(std::string{result}))
    {
      unsub_res = 0; // 0 denotes unsuccessfull
    }
    else
    {
      unsub_res = data["result"] ? 1 : 0;
      // if unsubscription was successful remove it from map
      if (unsub_res)
      {
        callback_map.erase(unsub_id);
        // TODO : more efficient way to implement this
        int key = -1;
        for (auto p : id_sub_map)
        {
          if (p.second == unsub_id)
          {
            key = p.first;
            break;
          }
        }
        id_sub_map.erase(key);
      }
    }
  }

  /// @brief call the specified callback
  /// @param data the data recieved from websocket
  void call_callback(json &data)
  {
    // if params is not a key then return
    std::string par = "params";
    if (!data.contains(std::string{par}))
    {
      return;
    }

    // if subscription is not a key in data["params"] then it's not relevant
    std::string subscription = "subscription";
    if (!data["params"].contains(std::string{subscription}))
    {
      return;
    }
    // call the specified callback related to the subscription id
    int returned_sub = data["params"]["subscription"];
    if (callback_map.find(returned_sub) != callback_map.end())
    {
      callback_map[returned_sub]->cb_(data);
    }
  }

  /// @brief close the connection from websocket
  /// @param ec the error code
  void on_close(beast::error_code ec)
  {
    if (ec)
      return fail(ec, "close");

    // If we get here then the connection is closed gracefully
    return;
  }

  // resolves the host and port provided by user
  tcp::resolver resolver_;

  // socket used by client to read and write
  websocket::stream<beast::tcp_stream> ws_;

  // strand of the io context of socket, useful in making requests in different
  // thread
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;

  // buffer to store data
  beast::flat_buffer buffer_;

  // host
  std::string host_;

  // pending subscription
  std::queue<std::shared_ptr<RequestContent>> pendingSub;

  // pending unsubscription
  std::queue<int> pendingUnSub;

  // map of subscription id with callback
  std::unordered_map<int, std::shared_ptr<RequestContent>> callback_map;

  // will work like a lock so that you don't do two async write simultaneously
  // will also be used to keep track of the type of message you are reading
  // response to write or a notification
  bool justSubscribed = false;
  bool justUnsubscribed = false;

  // subscription id on successfull subscription
  // -2 denotes processing subscription
  // -1 denotes subscription failed
  // rest everything is successful subscription id
  int sub_id = -2;

  // unsubscription result on successfull unsubscription
  // -1 denotes processing unsubscription
  // 0 denotes unsubscription failed
  // 1 denotes unsubscription successfull
  int unsub_res = -1;

  // denotes if the connection is up
  bool is_connected = false;

  // request content id with subscription map
  std::unordered_map<int, int> id_sub_map;
};
