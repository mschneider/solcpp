#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"

const solana::PublicKey DEFAULT_PUBLICKEY =
    solana::PublicKey::fromBase58("11111111111111111111111111111111");

int main() {
  std::string rpc_url =
      "https://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88";
  auto connection = solana::rpc::Connection(rpc_url);
  const std::string account = "98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue";
  const mango_v3::MangoGroup grp =
      connection
          .getAccountInfo<mango_v3::MangoGroup>(
              solana::PublicKey::fromBase58(account))
          .value.value()
          .data;
  const auto group = &grp;

  spdlog::info("DEC:");
  spdlog::info("numOracles: {}", group->numOracles);

  for (int i = 0; i < mango_v3::MAX_TOKENS; ++i) {
    if (i >= group->numOracles && i != mango_v3::QUOTE_INDEX) continue;

    const auto &token = group->tokens[i];
    const auto mintPk = token.mint;

    if (mintPk == DEFAULT_PUBLICKEY) continue;

    const auto rootBankPk = token.rootBank;
    if (i != mango_v3::QUOTE_INDEX) {
      spdlog::info("TOK: {}", i);
    } else {
      spdlog::info("QUOTE: {}", i);
    }
    spdlog::info("  mint: {}", mintPk.toBase58());
    spdlog::info("  rootBank: {}", rootBankPk.toBase58());
    spdlog::info("  decimals: {}", static_cast<int>(token.decimals));
  }

  for (int i = 0; i < mango_v3::MAX_PAIRS; ++i) {
    const auto &market = group->spotMarkets[i];
    const auto marketPk = market.spotMarket;

    if (marketPk == DEFAULT_PUBLICKEY) continue;

    spdlog::info("SPOT: {}", i);
    spdlog::info("  market: {}", marketPk.toBase58());
    spdlog::info("  maintAssetWeight: {}", market.maintAssetWeight.to_double());
    spdlog::info("  initAssetWeight: {}", market.initAssetWeight.to_double());
    spdlog::info("  maintLiabWeight: {}", market.maintLiabWeight.to_double());
    spdlog::info("  initLiabWeight: {}", market.initLiabWeight.to_double());
    spdlog::info("  liquidationFee: {}", market.liquidationFee.to_double());
  }

  for (int i = 0; i < mango_v3::MAX_PAIRS; ++i) {
    const auto &market = group->perpMarkets[i];
    const auto marketPk = market.perpMarket;

    if (marketPk == DEFAULT_PUBLICKEY) continue;

    spdlog::info("PERP: {}", i);
    spdlog::info("  market: {}", marketPk.toBase58());
    spdlog::info("  maintAssetWeight: {}", market.maintAssetWeight.to_double());
    spdlog::info("  initAssetWeight: {}", market.initAssetWeight.to_double());
    spdlog::info("  maintLiabWeight: {}", market.maintLiabWeight.to_double());
    spdlog::info("  initLiabWeight: {}", market.initLiabWeight.to_double());
    spdlog::info("  liquidationFee: {}", market.liquidationFee.to_double());
    spdlog::info("  makerFee: {}", market.makerFee.to_double());
    spdlog::info("  takerFee: {}", market.takerFee.to_double());
    spdlog::info("  baseLotSize: {}", market.baseLotSize);
    spdlog::info("  quoteLotSize: {}", market.quoteLotSize);
  }

  for (int i = 0; i < mango_v3::MAX_PAIRS; ++i) {
    const auto oraclePk = group->oracles[i];

    if (oraclePk == DEFAULT_PUBLICKEY) continue;

    spdlog::info("ORACLE {}: {}", i, oraclePk.toBase58());
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
