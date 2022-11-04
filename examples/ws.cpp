#include <nlohmann/json.hpp>

#include "solana.hpp"

using json = nlohmann::json;

bool subscribe_called = false;
void call_on_subscribe(json &data) { subscribe_called = !subscribe_called; }

int main() {
  solana::rpc::subscription::WebSocketSubscriber sub("127.0.0.1", "8900");
  int sub_id = sub.onAccountChange(
      "8vnAsXjHtdRgyuFHVESjfe1CBmWSwRFRgBR3WJCQbMiW", call_on_subscribe);
  // stop execution here and call solana airdrop 1 from terminal
  // sleep(10);
  std::cout << "Subscribe Called Value " << subscribe_called << std::endl;
  sub.removeAccountChangeListener(0);
  // call solana airdrop 1
  sleep(10);
  std::cout << "Subscribe Called Value " << subscribe_called << std::endl;
}