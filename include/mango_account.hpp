#pragma once

#include "mango_v3.hpp"
#include "utils.h"

namespace mango_v3 {
struct MangoAccount {
  MangoAccountInfo accountInfo;
  std::unordered_map<std::string, serum_v3::OpenOrders> spotOpenOrdersAccounts;
  explicit MangoAccount(const MangoAccountInfo& accountInfo_) noexcept {
    accountInfo = accountInfo_;
    // TODO: Call loadOpenOrders
  }
  // Fetch `accountInfo` from `endpoint` and decode it
  explicit MangoAccount(const std::string& pubKey,
                        solana::rpc::Connection& connection) {
    const auto& accountInfo_ =
        connection.getAccountInfo<MangoAccountInfo>(pubKey);
    accountInfo = accountInfo_;
  }
  std::unordered_map<std::string, serum_v3::OpenOrders> loadOpenOrders(
      solana::rpc::Connection& connection);
  // Return the spot, perps and quote currency values after adjusting for
  // worst case open orders scenarios. These values are not adjusted for health
  // type
  // TODO: Change `spot` and `perps` from vec to array for `getHealth()`
  std::tuple<std::vector<i80f48>, std::vector<i80f48>, i80f48>
  getHealthComponents(MangoGroup* mangoGroup, MangoCache* mangoCache);
  i80f48 getHealthFromComponents(MangoGroup* mangoGroup, MangoCache* mangoCache,
                                 std::vector<i80f48> spot,
                                 std::vector<i80f48> perps, i80f48 quote,
                                 HealthType healthType);
  // deposits - borrows in native terms
  i80f48 getNet(RootBankCache rootBankCache, size_t tokenIndex) {
    return (accountInfo.deposits[tokenIndex] * rootBankCache.deposit_index) -
           (accountInfo.borrows[tokenIndex] * rootBankCache.borrow_index);
  }
  i80f48 getHealth(MangoGroup* mangoGroup, MangoCache* mangoCache,
                   HealthType healthType);
  // Take health components and return the assets and liabs weighted
  std::pair<i80f48, i80f48> getWeightedAssetsLiabsVals(
      MangoGroup* mangoGroup, MangoCache* mangoCache, std::vector<i80f48> spot,
      std::vector<i80f48> perps, i80f48 quote,
      HealthType healthType = HealthType::Unknown);
  i80f48 getHealthRatio(MangoGroup* mangoGroup, MangoCache* mangoCache,
                        HealthType healthType = HealthType::Unknown);
  bool isLiquidatable(MangoGroup* mangoGroup, MangoCache* mangoCache);
  i80f48 computeValue(MangoGroup* mangoGroup, MangoCache* mangoCache);
  i80f48 getLeverage(MangoGroup* mangoGroup, MangoCache* mangoCache);
  i80f48 getAssetsVal(MangoGroup* mangoGroup, MangoCache* mangoCache,
                      HealthType healthType = HealthType::Unknown);
  i80f48 getLiabsVal(MangoGroup* mangoGroup, MangoCache* mangoCache,
                     HealthType healthType = HealthType::Unknown);
  i80f48 getUiBorrow(RootBankCache* rootBankCache, size_t tokenIndex);
  i80f48 getNativeBorrow(RootBankCache* rootBankCache, MangoGroup* mangoGroup,
                         size_t tokenIndex);
  i80f48 getUiDeposit(RootBankCache* rootBankCache, MangoGroup* mangoGroup,
                      size_t tokenIndex);
  i80f48 getSpotVal(MangoGroup* mangoGroup, MangoCache* mangoCache,
                    size_t index, i80f48 assetWeight);
};
std::unordered_map<std::string, serum_v3::OpenOrders>
MangoAccount::loadOpenOrders(solana::rpc::Connection& connection) {
  // Filter only non-empty open orders
  std::vector<std::string> filteredOpenOrders;
  for (auto item : accountInfo.spotOpenOrders) {
    if (item == solana::PublicKey::empty()) continue;
    filteredOpenOrders.emplace_back(std::move(item.toBase58()));
  }
  // Fetch account info, OpenOrdersV2
  const auto accountsInfo =
      connection.getMultipleAccounts<serum_v3::OpenOrders>(filteredOpenOrders);
  spotOpenOrdersAccounts.clear();
  std::copy_if(
      accountsInfo.begin(), accountsInfo.end(),
      std::inserter(spotOpenOrdersAccounts, spotOpenOrdersAccounts.end()),
      [](auto& accountInfo) {
        auto isInitialiazed = (accountInfo.second.accountFlags &
                               serum_v3::AccountFlags::Initialized) ==
                              serum_v3::AccountFlags::Initialized;
        auto isOpenOrders = (accountInfo.second.accountFlags &
                             serum_v3::AccountFlags::OpenOrders) ==
                            serum_v3::AccountFlags::OpenOrders;
        return (isInitialiazed && isOpenOrders);
      });
  return spotOpenOrdersAccounts;
}

std::tuple<std::vector<i80f48>, std::vector<i80f48>, i80f48>
MangoAccount::getHealthComponents(MangoGroup* mangoGroup,
                                  MangoCache* mangoCache) {
  std::vector<i80f48> spot(mangoGroup->numOracles, i80f48(0L));
  std::vector<i80f48> perps(mangoGroup->numOracles, i80f48(0L));
  auto quote = getNet(mangoCache->root_bank_cache[QUOTE_INDEX], QUOTE_INDEX);
  for (int i = 0; i < mangoGroup->numOracles; i++) {
    const auto bankCache = mangoCache->root_bank_cache[i];
    const auto price = mangoCache->price_cache[i].price;
    const auto baseNet = getNet(bankCache, i);

    // Evaluate spot first
    auto spotOpenOrdersKey = accountInfo.spotOpenOrders[i].toBase58();
    if ((spotOpenOrdersAccounts.find(spotOpenOrdersKey) !=
         spotOpenOrdersAccounts.end()) &&
        accountInfo.inMarginBasket[i]) {
      const auto openOrders = spotOpenOrdersAccounts.at(spotOpenOrdersKey);
      // C++17 structured bindings
      auto [quoteFree, quoteLocked, baseFree, baseLocked] =
          splitOpenOrders(&openOrders);

      // base total if all bids were executed
      const auto bidsBaseNet =
          baseNet + (quoteLocked / price) + baseFree + baseLocked;
      // base total if all asks were executed
      const auto asksBaseNet = baseNet + baseFree;
      // bids case worse if it has a higher absolute position
      if (abs(bidsBaseNet.toDouble()) > abs(asksBaseNet.toDouble())) {
        spot[i] = bidsBaseNet;
        quote += quoteFree;
      } else {
        spot[i] = asksBaseNet;
        quote += (baseLocked * price) + quoteFree + quoteLocked;
      }
    } else {
      spot[i] = baseNet;
    }
    // Evaluate perps
    if (!(mangoGroup->perpMarkets[i].perpMarket ==
          solana::PublicKey::empty())) {
      const auto perpMarketCache = mangoCache->perp_market_cache[i];
      const auto perpAccount = accountInfo.perpAccounts[i];
      const auto baseLotSize = mangoGroup->perpMarkets[i].baseLotSize;
      const auto quoteLotSize = mangoGroup->perpMarkets[i].quoteLotSize;
      const auto takerQuote = i80f48(perpAccount.takerQuote * quoteLotSize);
      const auto basePos = i80f48(
          ((perpAccount.basePosition + perpAccount.takerBase) * baseLotSize));
      auto bidsQuantity = i80f48((perpAccount.bidsQuantity * baseLotSize));
      auto asksQuantity = i80f48((perpAccount.asksQuantity * baseLotSize));
      const auto bidsBaseNet = basePos + bidsQuantity;
      const auto asksBaseNet = basePos - asksQuantity;
      if (abs(bidsBaseNet.toDouble()) > abs(asksBaseNet.toDouble())) {
        const auto quotePos =
            (getQuotePosition(&perpAccount, &perpMarketCache) + takerQuote) -
            (bidsQuantity * price);
        quote += quotePos;
        perps[i] = bidsBaseNet;
      } else {
        const auto quotePos = getQuotePosition(&perpAccount, &perpMarketCache) +
                              takerQuote + (asksQuantity * price);
        quote += quotePos;
        perps[i] = asksBaseNet;
      }

    } else {
      perps[i] = i80f48(0L);
    }
  }
  return std::make_tuple(spot, perps, quote);
}
i80f48 MangoAccount::getHealthFromComponents(
    MangoGroup* mangoGroup, MangoCache* mangoCache, std::vector<i80f48> spot,
    std::vector<i80f48> perps, i80f48 quote, HealthType healthType) {
  auto health = quote;
  for (int i = 0; i < mangoGroup->numOracles; i++) {
    const auto [spotAssetWeight, spotLiabWeight, perpAssetWeight,
                perpLiabWeight] = getWeights(mangoGroup, i, healthType);
    const auto price = mangoCache->price_cache[i].price;
    const auto spotHealth =
        (spot[i] * price) *
        (spot[i] > i80f48(0L) ? spotAssetWeight : spotLiabWeight);
    const auto perpHealth =
        (perps[i] * price) *
        (perps[i] > i80f48(0L) ? perpAssetWeight : perpLiabWeight);
    health += spotHealth;
    health += perpHealth;
    auto a = spot[i];
    a *= price;
    a *= (spot[i] > i80f48(0L) ? spotAssetWeight : spotLiabWeight);
  }
  return health;
}
i80f48 MangoAccount::getHealth(MangoGroup* mangoGroup, MangoCache* mangoCache,
                               HealthType healthType) {
  const auto [spot, perps, quote] = getHealthComponents(mangoGroup, mangoCache);
  const auto health = getHealthFromComponents(mangoGroup, mangoCache, spot,
                                              perps, quote, healthType);
  return health;
}
std::pair<i80f48, i80f48> MangoAccount::getWeightedAssetsLiabsVals(
    MangoGroup* mangoGroup, MangoCache* mangoCache, std::vector<i80f48> spot,
    std::vector<i80f48> perps, i80f48 quote, HealthType healthType) {
  auto assets = i80f48(0L);
  auto liabs = i80f48(0L);
  if (quote > 0) {
    assets += quote;
  } else {
    liabs += (quote * i80f48(-1L));
  }
  for (int i = 0; i < mangoGroup->numOracles; i++) {
    const auto [spotAssetWeight, spotLiabWeight, perpAssetWeight,
                perpLiabWeight] = getWeights(mangoGroup, i, healthType);
    const auto price = mangoCache->price_cache[i].price;
    if (spot[i] > i80f48(0L)) {
      assets += ((spot[i] * price * spotAssetWeight));
    } else {
      liabs += ((spot[i] * i80f48(-1L)) * price * spotLiabWeight);
    }

    if (perps[i] > i80f48(0L)) {
      assets += (perps[i] * price * perpAssetWeight);
    } else {
      liabs += ((perps[i] * i80f48(-1L)) * price * perpLiabWeight);
    }
  }
  return std::make_pair(assets, liabs);
}
i80f48 MangoAccount::getHealthRatio(MangoGroup* mangoGroup,
                                    MangoCache* mangoCache,
                                    HealthType healthType) {
  auto [spot, perps, quote] = getHealthComponents(mangoGroup, mangoCache);
  auto [assets, liabs] = getWeightedAssetsLiabsVals(
      mangoGroup, mangoCache, spot, perps, quote, healthType);
  if (liabs > i80f48(0L)) {
    return ((assets / liabs) - i80f48(1L)) * i80f48(100L);
  } else {
    return i80f48(100L);
  }
}

bool MangoAccount::isLiquidatable(MangoGroup* mangoGroup,
                                  MangoCache* mangoCache) {
  auto initHealth = getHealth(mangoGroup, mangoCache, HealthType::Init);
  auto maintHealth = getHealth(mangoGroup, mangoCache, HealthType::Maint);
  return (accountInfo.beingLiquidated && initHealth < 0 && maintHealth < 0);
}
i80f48 MangoAccount::computeValue(MangoGroup* mangoGroup,
                                  MangoCache* mangoCache) {
  return getAssetsVal(mangoGroup, mangoCache) -
         getLiabsVal(mangoGroup, mangoCache);
}
i80f48 MangoAccount::getLeverage(MangoGroup* mangoGroup,
                                 MangoCache* mangoCache) {
  auto liabs = getLiabsVal(mangoGroup, mangoCache);
  auto assets = getAssetsVal(mangoGroup, mangoCache);
  if (assets > i80f48(0ULL)) {
    return liabs / (assets - liabs);
  }
  return i80f48(0ULL);
}
i80f48 MangoAccount::getAssetsVal(MangoGroup* mangoGroup,
                                  MangoCache* mangoCache,
                                  HealthType healthType) {
  auto assetsVal = i80f48(0ULL);
  // quote currency deposits
  assetsVal += getUiDeposit(&mangoCache->root_bank_cache[QUOTE_INDEX],
                            mangoGroup, QUOTE_INDEX);
  for (int i = 0; i < mangoGroup->numOracles; i++) {
    auto assetWeight = i80f48(1ULL);
    if (healthType == HealthType::Maint) {
      assetWeight = mangoGroup->spotMarkets[i].maintAssetWeight;
    } else if (healthType == HealthType::Init) {
      assetWeight = mangoGroup->spotMarkets[i].initAssetWeight;
    }
    auto spotVal = getSpotVal(mangoGroup, mangoCache, i, assetWeight);
    assetsVal += spotVal;

    const auto price = mangoCache->price_cache[i].price;
    auto perpAssetVal =
        getAssetVal(&accountInfo.perpAccounts[i], &mangoGroup->perpMarkets[i],
                    price, mangoCache->perp_market_cache[i].short_funding,
                    mangoCache->perp_market_cache[i].long_funding);
    const auto perpsUiAssetVal = nativeI80F48ToUi(
        perpAssetVal, mangoGroup->tokens[QUOTE_INDEX].decimals);
    assetsVal += perpsUiAssetVal;
  }
  return assetsVal;
}
i80f48 MangoAccount::getLiabsVal(MangoGroup* mangoGroup, MangoCache* mangoCache,
                                 HealthType healthType) {

}
i80f48 MangoAccount::getUiBorrow(RootBankCache* rootBankCache,
                                 size_t tokenIndex) {}
i80f48 MangoAccount::getNativeBorrow(RootBankCache* rootBankCache,
                                     MangoGroup* mangoGroup,
                                     size_t tokenIndex) {}
i80f48 MangoAccount::getUiDeposit(RootBankCache* rootBankCache,
                                  MangoGroup* mangoGroup, size_t tokenIndex) {}
i80f48 MangoAccount::getSpotVal(MangoGroup* mangoGroup, MangoCache* mangoCache,
                                size_t index, i80f48 assetWeight) {}

} // namespace mango_v3