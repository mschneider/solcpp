#include <spdlog/spdlog.h>
#include "mango_v3.hpp"

using json = nlohmann::json;

int main(){
  const auto& config = mango_v3::DEVNET;
  auto connection = solana::rpc::Connection(config.endpoint);
  const json req = connection.getAccountInfoRequest(
    "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW");
  const std::string jsonSerialized = req.dump();

  cpr::Response r =
      cpr::Post(cpr::Url{config.endpoint}, cpr::Body{jsonSerialized.c_str()},
                cpr::Header{{"Content-Type", "application/json"}});

  if (r.status_code == 0 || r.status_code >= 400) {
    spdlog::error("Error: {}, {}", r.status_code, r.error.message);
    return 1;
  }
  json res = json::parse(r.text);
  const std::string encoded = res["result"]["value"]["data"][0];
  const std::string decoded = solana::b64decode(encoded);
  const mango_v3::MangoAccount *account =
      reinterpret_cast<const mango_v3::MangoAccount *>(decoded.data());
  spdlog::info(account->owner.toBase58());
  // TODO: #13
}