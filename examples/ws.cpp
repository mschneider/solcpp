#include <nlohmann/json.hpp>

#include "solana.hpp"

using json = nlohmann::json;

int main() {
  solana::rpc::subscription::WebSocketSubscriber sub("127.0.0.1", "8900");
  bool subscribe_called = false;
  auto call_on_subscribe = [&subscribe_called](const json&) {
    subscribe_called = true;
  };
  int sub_id = sub.onAccountChange(
      "8vnAsXjHtdRgyuFHVESjfe1CBmWSwRFRgBR3WJCQbMiW", call_on_subscribe);
  // stop execution here and call solana airdrop 1 from terminal
  sleep(20);
  std::cout << "Subscribe Called Value " << subscribe_called << std::endl;
  sub.removeAccountChangeListener(sub_id);
  // call solana airdrop 1
  sleep(10);
  std::cout << "Subscribe Called Value " << subscribe_called << std::endl;
}