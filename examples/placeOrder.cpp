#include <spdlog/spdlog.h>

#include "mango_v3.hpp"

int main() {
  const auto &config = mango_v3::DEVNET;
  auto connection = solana::rpc::Connection(config.endpoint);
  const auto group =
      connection.getAccountInfo<mango_v3::MangoGroup>(config.group);

  const auto symbolIt =
      std::find(config.symbols.begin(), config.symbols.end(), "BTC");
  const auto marketIndex = symbolIt - config.symbols.begin();
  assert(config.symbols[marketIndex] == "BTC");

  const auto perpMarketPk = group.perpMarkets[marketIndex].perpMarket;

  const auto market =
      connection.getAccountInfo<mango_v3::PerpMarket>(perpMarketPk.toBase58());
  assert(market.mangoGroup.toBase58() == config.group);

  const auto recentBlockhash = connection.getRecentBlockhash();
  const auto groupPk = solana::PublicKey::fromBase58(config.group);
  const auto programPk = solana::PublicKey::fromBase58(config.program);
  const auto keypair =
      solana::Keypair::fromFile("../tests/fixtures/solana/id.json");
  const auto mangoAccount = solana::PublicKey::fromBase58(
      "9aWg1jhgRzGRmYWLbTrorCFE7BQbaz2dE5nYKmqeLGCW");

  const mango_v3::ix::CancelAllPerpOrders cancelData = {
      mango_v3::ix::CancelAllPerpOrders::CODE, 4};

  const auto pqBid = mango_v3::ix::uiToNativePriceQuantity(31000, 0.01, config,
                                                           marketIndex, market);

  const mango_v3::ix::PlacePerpOrder placeBidData = {
      mango_v3::ix::PlacePerpOrder::CODE,
      pqBid.first,
      pqBid.second,
      1,
      mango_v3::Side::Buy,
      mango_v3::ix::OrderType::Limit,
      false};

  const auto pqAsk = mango_v3::ix::uiToNativePriceQuantity(59000, 0.01, config,
                                                           marketIndex, market);

  const mango_v3::ix::PlacePerpOrder placeAskData = {
      mango_v3::ix::PlacePerpOrder::CODE,
      pqAsk.first,
      pqAsk.second,
      2,
      mango_v3::Side::Sell,
      mango_v3::ix::OrderType::Limit,
      false};

  const std::vector<solana::Instruction> ixs = {
      cancelAllPerpOrdersInstruction(cancelData, keypair.publicKey,
                                     mangoAccount, perpMarketPk, market,
                                     groupPk, programPk),
      placePerpOrderInstruction(placeBidData, keypair.publicKey, mangoAccount,
                                perpMarketPk, market, groupPk, group,
                                programPk),
      placePerpOrderInstruction(placeAskData, keypair.publicKey, mangoAccount,
                                perpMarketPk, market, groupPk, group,
                                programPk)};

  const auto tx = solana::CompiledTransaction::fromInstructions(
      ixs, keypair.publicKey, recentBlockhash);

  const auto b58Sig = connection.signAndSendTransaction(keypair, tx);
  spdlog::info(
      "placed order. check: https://explorer.solana.com/tx/{}?cluster=devnet",
      b58Sig);

  return 0;
}
