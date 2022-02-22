#include <chrono>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../mango_v3.hpp"

int main()
{
  const auto &config = mango_v3::MAINNET;
  const auto group = solana::rpc::getAccountInfo<mango_v3::MangoGroup>(
      config.endpoint,
      config.group);

  const auto symbolIt = std::find(config.symbols.begin(), config.symbols.end(), "BTC");
  const auto marketIndex = symbolIt - config.symbols.begin();
  assert(config.symbols[marketIndex] == "BTC");

  const auto perpMarketPk = group.perpMarkets[marketIndex].perpMarket;

  const auto market = solana::rpc::getAccountInfo<mango_v3::PerpMarket>(
      config.endpoint,
      perpMarketPk.toBase58());
  assert(market.mangoGroup.toBase58() == config.group);

  while (true)
  {
    const auto bids = solana::rpc::getAccountInfo<mango_v3::BookSide>(config.endpoint, market.bids.toBase58());

    auto bidsIt = mango_v3::BookSide::iterator(mango_v3::Side::Buy, bids);
    while (bidsIt.stack.size() > 0)
    {
      if ((*bidsIt).tag == mango_v3::NodeType::LeafNode)
      {
        const auto leafNode = reinterpret_cast<const struct mango_v3::LeafNode *>(&(*bidsIt));
        const auto now = std::chrono::system_clock::now();
        const auto nowUnix = chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        const auto isValid = !leafNode->timeInForce || leafNode->timestamp + leafNode->timeInForce < nowUnix;
        if (isValid)
        {
          std::cout << "best bid prz:" << (uint64_t)(leafNode->key >> 64) << " qty:" << leafNode->quantity << std::endl;
          break;
        }
      }
      ++bidsIt;
    }
  }
}