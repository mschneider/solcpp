#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <nlohmann/json.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"

using json = nlohmann::json;

int main() {
  const auto &config = mango_v3::MAINNET;
  // todo: make this const
  solana::rpc::Connection solCon;
  const auto group = solCon.getAccountInfo<mango_v3::MangoGroup>(config.group);

  const auto symbolIt =
      std::find(config.symbols.begin(), config.symbols.end(), "BTC");
  const auto marketIndex = symbolIt - config.symbols.begin();
  assert(config.symbols[marketIndex] == "BTC");

  const auto perpMarketPk = group.perpMarkets[marketIndex].perpMarket;

  const auto market =
      solCon.getAccountInfo<mango_v3::PerpMarket>(perpMarketPk.toBase58());
  assert(market.mangoGroup.toBase58() == config.group);

  while (true) {
    const auto bids =
        solCon.getAccountInfo<mango_v3::BookSide>(market.bids.toBase58());

    auto bidsIt = mango_v3::BookSide::iterator(mango_v3::Side::Buy, bids);
    while (bidsIt.stack.size() > 0) {
      if ((*bidsIt).tag == mango_v3::NodeType::LeafNode) {
        const auto leafNode =
            reinterpret_cast<const struct mango_v3::LeafNode *>(&(*bidsIt));
        const auto now = std::chrono::system_clock::now();
        const auto nowUnix =
            chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                .count();
        const auto isValid =
            !leafNode->timeInForce ||
            leafNode->timestamp + leafNode->timeInForce < nowUnix;
        if (isValid) {
          spdlog::info("best bid price: {} qty: {}",
                       (uint64_t)(leafNode->key >> 64), leafNode->quantity);
          break;
        }
      }
      ++bidsIt;
    }
  }
}
