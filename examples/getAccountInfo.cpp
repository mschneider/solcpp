#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "mango_v3.hpp"

int main() {
  std::string rpc_url =
      "https://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88";
  auto connection = solana::rpc::Connection(rpc_url);
  const json req = connection.getAccountInfoRequest(
      "98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue");
  const std::string jsonSerialized = req.dump();
  spdlog::info("REQ: {}", jsonSerialized);

  cpr::Response r =
      cpr::Post(cpr::Url{rpc_url}, cpr::Body{jsonSerialized.c_str()},
                cpr::Header{{"Content-Type", "application/json"}});

  if (r.status_code == 0 || r.status_code >= 400) {
    spdlog::error("Error: {}, {}", r.status_code, r.error.message);
    return 1;
  } else {
    spdlog::info("RES: {}", r.text);
    json res = json::parse(r.text);

    const std::string encoded = res["result"]["value"]["data"][0];
    const std::string decoded = solana::b64decode(encoded);
    const mango_v3::MangoGroup *group =
        reinterpret_cast<const mango_v3::MangoGroup *>(decoded.data());
    spdlog::info("DEC:");
    spdlog::info("numOracles: {}", group->numOracles);

    for (int i = 0; i < mango_v3::MAX_TOKENS; ++i) {
      if (i >= group->numOracles && i != mango_v3::QUOTE_INDEX) continue;

      const auto &token = group->tokens[i];
      const auto mintPk = token.mint.toBase58();

      if (mintPk == std::string("11111111111111111111111111111111")) continue;

      const auto rootBankPk = token.rootBank.toBase58();
      if (i != mango_v3::QUOTE_INDEX) {
        spdlog::info("TOK: {}", i);
      } else {
        spdlog::info("QUOTE: {}", i);
      }
      spdlog::info("  mint: {}", mintPk);
      spdlog::info("  rootBank: {}", rootBankPk);
      spdlog::info("  decimals: {}", static_cast<int>(token.decimals));
    }

    for (int i = 0; i < mango_v3::MAX_PAIRS; ++i) {
      const auto &market = group->spotMarkets[i];
      const auto marketPk = market.spotMarket.toBase58();

      if (marketPk == std::string("11111111111111111111111111111111")) continue;

      spdlog::info("SPOT: {}", i);
      spdlog::info("  market: {}", marketPk);
      spdlog::info("  maintAssetWeight: {}",
                   market.maintAssetWeight.toDouble());
      spdlog::info("  initAssetWeight: {}", market.initAssetWeight.toDouble());
      spdlog::info("  maintLiabWeight: {}", market.maintLiabWeight.toDouble());
      spdlog::info("  initLiabWeight: {}", market.initLiabWeight.toDouble());
      spdlog::info("  liquidationFee: {}", market.liquidationFee.toDouble());
    }

    for (int i = 0; i < mango_v3::MAX_PAIRS; ++i) {
      const auto &market = group->perpMarkets[i];
      const auto marketPk = market.perpMarket.toBase58();

      if (marketPk == std::string("11111111111111111111111111111111")) continue;

      spdlog::info("PERP: {}", i);
      spdlog::info("  market: {}", marketPk);
      spdlog::info("  maintAssetWeight: {}",
                   market.maintAssetWeight.toDouble());
      spdlog::info("  initAssetWeight: {}", market.initAssetWeight.toDouble());
      spdlog::info("  maintLiabWeight: {}", market.maintLiabWeight.toDouble());
      spdlog::info("  initLiabWeight: {}", market.initLiabWeight.toDouble());
      spdlog::info("  liquidationFee: {}", market.liquidationFee.toDouble());
      spdlog::info("  makerFee: {}", market.makerFee.toDouble());
      spdlog::info("  takerFee: {}", market.takerFee.toDouble());
      spdlog::info("  baseLotSize: {}", market.baseLotSize);
      spdlog::info("  quoteLotSize: {}", market.quoteLotSize);
    }

    for (int i = 0; i < mango_v3::MAX_PAIRS; ++i) {
      const auto oraclePk = group->oracles[i].toBase58();

      if (oraclePk == std::string("11111111111111111111111111111111")) continue;

      spdlog::info("ORACLE {}: {}", i, oraclePk);
    }

    spdlog::info("signerNonce: {}", group->signerNonce);
    spdlog::info("signerKey: {}", group->signerKey.toBase58());
    spdlog::info("admin: {}", group->admin.toBase58());
    spdlog::info("dexProgramId: {}", group->dexProgramId.toBase58());
    spdlog::info("mangoCache: {}", group->mangoCache.toBase58());
    spdlog::info("validInterval: {}", group->validInterval);
    spdlog::info("insuranceVault: {}", group->insuranceVault.toBase58());
    spdlog::info("srmVault: {}", group->srmVault.toBase58());
    spdlog::info("msrmVault: {}", group->msrmVault.toBase58());
    spdlog::info("feesVault: {}", group->feesVault.toBase58());
    spdlog::info("maxMangoAccounts: {}", group->maxMangoAccounts);
    spdlog::info("numMangoAccounts: {}", group->numMangoAccounts);
  }
  return 0;
}
