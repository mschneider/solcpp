#include <spdlog/spdlog.h>

#include "mango_v3.hpp"

using json = nlohmann::json;

int main() {
  const auto& config = mango_v3::DEVNET;
  auto connection = solana::rpc::Connection(config.endpoint);
  const auto& mangoAccountInfo = connection.getAccountInfo<mango_v3::MangoAccountInfo>(
      "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW");
  const auto& mangoAccount = mango_v3::MangoAccount(mangoAccountInfo);
  spdlog::info(mangoAccountInfo.owner.toBase58());
  spdlog::info(mangoAccount.accountInfo.owner.toBase58());
  // TODO: #13
}