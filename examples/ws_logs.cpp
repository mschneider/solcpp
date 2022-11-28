#include <nlohmann/json.hpp>

#include "solana.hpp"

using json = nlohmann::json;

int main() {
  solana::rpc::subscription::WebSocketSubscriber sub("127.0.0.1", "8900");
  solana::rpc::Connection connection("http://127.0.0.1:8899");
  bool subscribe_called = false;
  bool unsubscribe_called = false;
  auto call_on_subscribe = [&subscribe_called](const json& j) {
    subscribe_called = true;
    std::cout << j << std::endl;
  };

  auto call_on_unsubscribed = [&unsubscribe_called](const json& j) {
    unsubscribe_called = true;
    std::cout << j << std::endl;
  };

  int callback_called = 0;
  auto on_callback = [&callback_called](const json& j) {
    std::cout << j << std::endl;
    ++callback_called;
  };

  std::cout << "Initailizing ws" << std::endl;
  const solana::PublicKey pk = solana::PublicKey::fromBase58(
      "8vnAsXjHtdRgyuFHVESjfe1CBmWSwRFRgBR3WJCQbMiW");
  int sub_id = sub.onLogs(on_callback, solana::Commitment::CONFIRMED,
                          solana::rpc::subscription::LogsFilter::ALL,
                          call_on_subscribe, call_on_unsubscribed);

  for (int i = 0; i < 1000; ++i)  // wait for subscription for 10 sec
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (subscribe_called) break;
  }
  if (!subscribe_called) {
    std::cerr << "subscription unsuccesful" << std::endl;
    return -1;
  }

  std::cout << "Start airdops" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(20));

  assert(unsubscribe_called == false);
  std::cout << "callback called should be equal to number of airdrops result "
            << callback_called << std::endl;
  std::cout << "Unsubscribing" << std::endl;
  sub.removeOnLogsListener(sub_id);
  // call solana airdrop 1
  for (int i = 0; i < 1000; ++i)  // wait for subscription for 10 sec
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (unsubscribe_called) break;
  }
  if (!unsubscribe_called) {
    std::cerr << "unsubscription unsuccesful" << std::endl;
    return -1;
  }
}