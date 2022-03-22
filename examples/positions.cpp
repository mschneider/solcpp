#include <spdlog/spdlog.h>

#include "mango_v3.hpp"

using json = nlohmann::json;

int main() {
  const auto &config = mango_v3::DEVNET;
  auto connection = solana::rpc::Connection(config.endpoint);
  auto account = connection.getAccountInfo<mango_v3::MangoAccount>(
      "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW");
  spdlog::info(account.owner.toBase58());
  // TODO: #13
}