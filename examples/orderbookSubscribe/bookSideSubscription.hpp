#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
#include <thread>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"

typedef websocketpp::client<websocketpp::config::asio_tls_client> ws_client;
typedef websocketpp::config::asio_client::message_type::ptr ws_message_ptr;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;

namespace examples {

using json = nlohmann::json;
class bookSideSubscription {
 public:
  bookSideSubscription(mango_v3::Side side, const std::string& account)
      : side(side), account(account) {
    runThread = std::thread(&bookSideSubscription::subscribe, this);
  }

  ~bookSideSubscription() { runThread.join(); }

  // todo: add public interface to get latest best price and so on
 private:
  ws_client c;
  const mango_v3::Side side;
  const std::string account;
  uint64_t bestPrice = 0;
  uint64_t quantity = 0;
  std::thread runThread;

  bool subscribe() {
    try {
      c.set_access_channels(websocketpp::log::alevel::all);
      c.init_asio();
      c.set_tls_init_handler(
          websocketpp::lib::bind(&bookSideSubscription::on_tls_init, this));

      c.set_open_handler(
          websocketpp::lib::bind(&bookSideSubscription::on_open, this,
                                 websocketpp::lib::placeholders::_1));
      c.set_message_handler(
          websocketpp::lib::bind(&bookSideSubscription::on_message, this,
                                 websocketpp::lib::placeholders::_1,
                                 websocketpp::lib::placeholders::_2));

      websocketpp::lib::error_code ec;
      ws_client::connection_ptr con = c.get_connection(
          "wss://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88", ec);
      if (ec) {
        spdlog::error("could not create connection because: {}", ec.message());
        return 0;
      }
      c.connect(con);
      c.run();
    } catch (websocketpp::exception const& e) {
      std::cout << e.what() << std::endl;
    }
    return true;
    // todo: return false if failed
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
    // subscribe to btc-perp eventQ
    websocketpp::lib::error_code ec;

    c.send(hdl,
           solana::rpc::subscription::accountSubscribeRequest(account).dump(),
           websocketpp::frame::opcode::value::text, ec);
    if (ec) {
      spdlog::error("subscribe failed because: {}", ec.message());
    } else {
      spdlog::info("subscribed to account {} for {}", account,
                   side ? "asks" : "bids");
    }
  }

  void on_message(websocketpp::connection_hdl hdl, ws_message_ptr msg) {
    const json parsedMsg = json::parse(msg->get_payload());
    // ignore subscription confirmation
    const auto itResult = parsedMsg.find("result");
    if (itResult != parsedMsg.end()) {
      spdlog::info("on_result {}", parsedMsg.dump());
      return;
    }

    const std::string encoded =
        parsedMsg["params"]["result"]["value"]["data"][0];
    const std::string decoded = solana::b64decode(encoded);
    if (decoded.size() != sizeof(mango_v3::BookSide))
      throw std::runtime_error("invalid response length " +
                               std::to_string(decoded.size()) + " expected " +
                               std::to_string(sizeof(mango_v3::BookSide)));

    mango_v3::BookSide bookSide;
    memcpy(&bookSide, decoded.data(), sizeof(decltype(bookSide)));

    auto iter = mango_v3::BookSide::iterator(side, bookSide);

    while (iter.stack.size() > 0) {
      if ((*iter).tag == mango_v3::NodeType::LeafNode) {
        const auto leafNode =
            reinterpret_cast<const struct mango_v3::LeafNode*>(&(*iter));
        const auto now = std::chrono::system_clock::now();
        const auto nowUnix =
            chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                .count();
        const auto isValid =
            !leafNode->timeInForce ||
            leafNode->timestamp + leafNode->timeInForce < nowUnix;
        if (isValid) {
          bestPrice = (uint64_t)(leafNode->key >> 64);
          quantity = leafNode->quantity;
          logBest();
          break;
        }
      }
      ++iter;
    }
  }

  void logBest() {
    // todo: if decided to make public, have to add mtx/atomic to avoid data
    // race
    spdlog::info("{} best price: {} quantity: {}", side ? "asks" : "bids",
                 bestPrice, quantity);
  }
};
}  // namespace examples
