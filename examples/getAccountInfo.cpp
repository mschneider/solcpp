#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "mango_v3.hpp"

int main()
{
  std::string rpc_url = "https://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88";
  auto solana = solana::rpc::Solana(rpc_url);
  const json req = solana.getAccountInfoRequest("98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue");
  const std::string jsonSerialized = req.dump();
  std::cout << "REQ: " << jsonSerialized << std::endl;

  cpr::Response r = cpr::Post(cpr::Url{rpc_url},
                              cpr::Body{jsonSerialized.c_str()},
                              cpr::Header{{"Content-Type", "application/json"}});

  if(r.status_code == 0)
  {
    std::cerr << r.error.message << std::endl;
    return 1;
  }
  else if (r.status_code >= 400)
  {
    std::cerr << "Error [" << r.status_code << "] making request" << std::endl;
    return 1;
  }
  else
  {
    std::cout << "RES: " << r.text << std::endl;
    json res = json::parse(r.text);

    const std::string encoded = res["result"]["value"]["data"][0];
    const std::string decoded = solana::b64decode(encoded);
    const mango_v3::MangoGroup *group = reinterpret_cast<const mango_v3::MangoGroup *>(decoded.data());
    std::cout << "DEC: " << std::endl;
    std::cout << "numOracles: " << group->numOracles << std::endl;

    for (int i = 0; i < mango_v3::MAX_TOKENS; ++i)
    {
      if (i >= group->numOracles && i != mango_v3::QUOTE_INDEX)
        continue;

      const auto &token = group->tokens[i];
      const auto mintPk = token.mint.toBase58();

      if (mintPk == std::string("11111111111111111111111111111111"))
        continue;

      const auto rootBankPk = token.rootBank.toBase58();
      if (i != mango_v3::QUOTE_INDEX)
        std::cout << "TOK" << i << std::endl;
      else
        std::cout << "QUOTE" << i << std::endl;
      std::cout << "  mint: " << mintPk << std::endl;
      std::cout << "  rootBank: " << rootBankPk << std::endl;
      std::cout << "  decimals: " << (int)token.decimals << std::endl;
    }

    for (int i = 0; i < mango_v3::MAX_PAIRS; ++i)
    {
      const auto &market = group->spotMarkets[i];
      const auto marketPk = market.spotMarket.toBase58();

      if (marketPk == std::string("11111111111111111111111111111111"))
        continue;

      std::cout << "SPOT" << i << std::endl;
      std::cout << "  market: " << marketPk << std::endl;
      std::cout << "  maintAssetWeight: " << market.maintAssetWeight.toDouble() << std::endl;
      std::cout << "  initAssetWeight: " << market.initAssetWeight.toDouble() << std::endl;
      std::cout << "  maintLiabWeight: " << market.maintLiabWeight.toDouble() << std::endl;
      std::cout << "  initLiabWeight: " << market.initLiabWeight.toDouble() << std::endl;
      std::cout << "  liquidationFee: " << market.liquidationFee.toDouble() << std::endl;
    }

    for (int i = 0; i < mango_v3::MAX_PAIRS; ++i)
    {
      const auto &market = group->perpMarkets[i];
      const auto marketPk = market.perpMarket.toBase58();

      if (marketPk == std::string("11111111111111111111111111111111"))
        continue;

      std::cout << "PERP" << i << std::endl;
      std::cout << "  market: " << marketPk << std::endl;
      std::cout << "  maintAssetWeight: " << market.maintAssetWeight.toDouble() << std::endl;
      std::cout << "  initAssetWeight: " << market.initAssetWeight.toDouble() << std::endl;
      std::cout << "  maintLiabWeight: " << market.maintLiabWeight.toDouble() << std::endl;
      std::cout << "  initLiabWeight: " << market.initLiabWeight.toDouble() << std::endl;
      std::cout << "  liquidationFee: " << market.liquidationFee.toDouble() << std::endl;
      std::cout << "  makerFee: " << market.makerFee.toDouble() << std::endl;
      std::cout << "  takerFee: " << market.takerFee.toDouble() << std::endl;
      std::cout << "  baseLotSize: " << market.baseLotSize << std::endl;
      std::cout << "  quoteLotSize: " << market.quoteLotSize << std::endl;
    }

    for (int i = 0; i < mango_v3::MAX_PAIRS; ++i)
    {
      const auto oraclePk = group->oracles[i].toBase58();

      if (oraclePk == std::string("11111111111111111111111111111111"))
        continue;

      std::cout << "ORACLE" << i << ": " << oraclePk << std::endl;
    }

    std::cout << "signerNonce: " << group->signerNonce << std::endl;
    std::cout << "signerKey: " << group->signerKey.toBase58() << std::endl;
    std::cout << "admin: " << group->admin.toBase58() << std::endl;
    std::cout << "dexProgramId: " << group->dexProgramId.toBase58() << std::endl;
    std::cout << "mangoCache: " << group->mangoCache.toBase58() << std::endl;
    std::cout << "validInterval: " << group->validInterval << std::endl;
    std::cout << "insuranceVault: " << group->insuranceVault.toBase58() << std::endl;
    std::cout << "srmVault: " << group->srmVault.toBase58() << std::endl;
    std::cout << "msrmVault: " << group->msrmVault.toBase58() << std::endl;
    std::cout << "feesVault: " << group->feesVault.toBase58() << std::endl;
    std::cout << "maxMangoAccounts: " << group->maxMangoAccounts << std::endl;
    std::cout << "numMangoAccounts: " << group->numMangoAccounts << std::endl;
  }
  return 0;
}
