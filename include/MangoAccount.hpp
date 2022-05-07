#pragma once

#include "mango_v3.hpp"
#include "utils.h"

namespace mango_v3 {
struct MangoAccount {
  MangoAccountInfo mangoAccountInfo;
  // {address, OpenOrders}
  std::unordered_map<std::string, serum_v3::OpenOrders> spotOpenOrdersAccounts;
  explicit MangoAccount(const MangoAccountInfo& accountInfo_) noexcept {
    mangoAccountInfo = accountInfo_;
    // TODO: Call `loadOpenOrders()`
  }
  explicit MangoAccount(const std::string& pubKey,
                        solana::rpc::Connection& connection) {
    auto accountInfo_ = connection.getAccountInfo<MangoAccountInfo>(pubKey);
    mangoAccountInfo = accountInfo_;
  }
  // Returns map(Address: OpenOrders) and sets this accounts
  // `spotOpenOrdersAccounts`
  auto loadOpenOrders(solana::rpc::Connection& connection) {
    // Filter only non-empty open orders
    std::vector<std::string> filteredOpenOrders;
    for (auto item : mangoAccountInfo.spotOpenOrders) {
      if (item == solana::PublicKey::empty()) continue;
      filteredOpenOrders.emplace_back(item.toBase58());
    }
    const auto accountsInfo =
        connection.getMultipleAccounts<serum_v3::OpenOrders>(
            filteredOpenOrders);
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

  /**
   * deposits - borrows in native terms
   */
  double getNet(const RootBankCache& cache, uint64_t tokenIndex) {
    return (mangoAccountInfo.deposits[tokenIndex].toDouble() *
            cache.deposit_index.toDouble()) -
           (mangoAccountInfo.borrows[tokenIndex].toDouble() *
            cache.borrow_index.toDouble());
  }
  /**
   * Return the spot, perps and quote currency values after adjusting for
   * worst case open orders scenarios. These values are not adjusted for health
   * type
   * @param mangoGroup
   * @param mangoCache
   */
  auto getHealthComponents(const MangoGroup& mangoGroup,
                           const MangoCache& mangoCache) {
    std::vector<double> spot(mangoGroup.numOracles, 0);
    std::vector<double> perps(mangoGroup.numOracles, 0);
    auto quote = getNet(mangoCache.root_bank_cache[QUOTE_INDEX], QUOTE_INDEX);
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      const auto bankCache = mangoCache.root_bank_cache[i];
      const auto price = mangoCache.price_cache[i].price.toDouble();
      const auto baseNet = getNet(bankCache, i);

      // Evaluate spot first
      auto spotOpenOrdersKey = mangoAccountInfo.spotOpenOrders[i].toBase58();
      if ((spotOpenOrdersAccounts.find(spotOpenOrdersKey) !=
           spotOpenOrdersAccounts.end()) &&
          mangoAccountInfo.inMarginBasket[i]) {
        const auto openOrders = spotOpenOrdersAccounts.at(spotOpenOrdersKey);
        // C++17 structured bindings :)
        auto [quoteFree, quoteLocked, baseFree, baseLocked] =
            splitOpenOrders(openOrders);

        // base total if all bids were executed
        const auto bidsBaseNet =
            baseNet + (quoteLocked / price) + baseFree + baseLocked;
        // base total if all asks were executed
        const auto asksBaseNet = baseNet + baseFree;
        // bids case worse if it has a higher absolute position
        if (abs(bidsBaseNet) > abs(asksBaseNet)) {
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
      if (!(mangoGroup.perpMarkets[i].perpMarket ==
            solana::PublicKey::empty())) {
        const auto perpMarketCache = mangoCache.perp_market_cache[i];
        const auto perpAccount = mangoAccountInfo.perpAccounts[i];
        const auto baseLotSize = mangoGroup.perpMarkets[i].baseLotSize;
        const auto quoteLotSize = mangoGroup.perpMarkets[i].quoteLotSize;
        const auto takerQuote = perpAccount.takerQuote * quoteLotSize;
        const auto basePos =
            ((perpAccount.basePosition + perpAccount.takerBase) * baseLotSize);
        auto bidsQuantity = perpAccount.bidsQuantity * baseLotSize;
        auto asksQuantity = perpAccount.asksQuantity * baseLotSize;
        const auto bidsBaseNet = basePos + bidsQuantity;
        const auto asksBaseNet = basePos - asksQuantity;
        if (abs(bidsBaseNet) > abs(asksBaseNet)) {
          const auto quotePos =
              (getQuotePosition(perpAccount, perpMarketCache) + takerQuote) -
              (bidsQuantity * price);
          quote += quotePos;
          perps[i] = bidsBaseNet;
        } else {
          const auto quotePos = getQuotePosition(perpAccount, perpMarketCache) +
                                takerQuote + (asksQuantity * price);
          quote += quotePos;
          perps[i] = asksBaseNet;
        }

      } else {
        perps[i] = 0;
      }
    }
    return std::make_tuple(spot, perps, quote);
  }
  double getHealthFromComponents(const MangoGroup& mangoGroup,
                                 const MangoCache& mangoCache,
                                 std::vector<double> spot,
                                 std::vector<double> perps, double quote,
                                 HealthType healthType) {
    auto health = quote;
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      const auto [spotAssetWeight, spotLiabWeight, perpAssetWeight,
                  perpLiabWeight] =
          getMangoGroupWeights(mangoGroup, i, healthType);
      const auto price = mangoCache.price_cache[i].price.toDouble();
      const auto spotHealth =
          (spot[i] * price) * (spot[i] > 0 ? spotAssetWeight.toDouble()
                                           : spotLiabWeight.toDouble());
      const auto perpHealth =
          (perps[i] * price) * (perps[i] > 0 ? perpAssetWeight.toDouble()
                                             : perpLiabWeight.toDouble());
      health += spotHealth;
      health += perpHealth;
      auto a = spot[i];
      a *= price;
      a *= (spot[i] > 0 ? spotAssetWeight.toDouble()
                        : spotLiabWeight.toDouble());
    }
    return health;
  }
  double getHealth(const MangoGroup& mangoGroup, const MangoCache& mangoCache,
                   HealthType healthType) {
    const auto [spot, perps, quote] =
        getHealthComponents(mangoGroup, mangoCache);
    const auto health = getHealthFromComponents(mangoGroup, mangoCache, spot,
                                                perps, quote, healthType);
    return health;
  }
  /**
   * Take health components and return the assets and liabs weighted
   */
  auto getWeightedAssetsLiabsVals(const MangoGroup& mangoGroup,
                                  const MangoCache& mangoCache,
                                  std::vector<double> spot,
                                  std::vector<double> perps, double quote,
                                  HealthType healthType) {
    double assets = 0;
    double liabs = 0;
    if (quote > 0) {
      assets += quote;
    } else {
      liabs += (quote * -1);
    }
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      const auto [spotAssetWeight, spotLiabWeight, perpAssetWeight,
                  perpLiabWeight] =
          getMangoGroupWeights(mangoGroup, i, healthType);
      const auto price = mangoCache.price_cache[i].price.toDouble();
      if (spot[i] > 0) {
        assets += ((spot[i] * price * spotAssetWeight.toDouble()));
      } else {
        liabs += ((spot[i] * -1) * price * spotLiabWeight.toDouble());
      }

      if (perps[i] > 0) {
        assets += (perps[i] * price * perpAssetWeight.toDouble());
      } else {
        liabs += (perps[i] * -1 * price * perpLiabWeight.toDouble());
      }
    }
    return std::make_pair(assets, liabs);
  }
  double getHealthRatio(const MangoGroup& mangoGroup,
                        const MangoCache& mangoCache, HealthType healthType) {
    auto [spot, perps, quote] = getHealthComponents(mangoGroup, mangoCache);
    auto [assets, liabs] = getWeightedAssetsLiabsVals(
        mangoGroup, mangoCache, spot, perps, quote, healthType);
    if (liabs > 0) {
      return ((assets / liabs) - 1) * 100;
    } else {
      return 100;
    }
  }

  bool isLiquidatable(const MangoGroup& mangoGroup,
                      const MangoCache& mangoCache) {
    auto initHealth = getHealth(mangoGroup, mangoCache, HealthType::Init);
    auto maintHealth = getHealth(mangoGroup, mangoCache, HealthType::Maint);
    return ((mangoAccountInfo.beingLiquidated && initHealth < 0) ||
            (maintHealth < 0));
  }
  double computeValue(const MangoGroup& mangoGroup,
                      const MangoCache& mangoCache) {
    auto a = getAssetsVal(mangoGroup, mangoCache, HealthType::Unknown);
    auto b = getLiabsVal(mangoGroup, mangoCache, HealthType::Unknown);
    return a - b;
  }
  double getLeverage(const MangoGroup& mangoGroup,
                     const MangoCache& mangoCache) {
    auto liabs = getLiabsVal(mangoGroup, mangoCache, HealthType::Unknown);
    auto assets = getAssetsVal(mangoGroup, mangoCache, HealthType::Unknown);
    if (assets > 0) {
      return liabs / (assets - liabs);
    }
    return 0;
  }
  double getAssetsVal(const MangoGroup& mangoGroup,
                      const MangoCache& mangoCache, HealthType healthType) {
    double assetsVal = 0;
    // quote currency deposits
    assetsVal += getUiDeposit(mangoCache.root_bank_cache[QUOTE_INDEX],
                              mangoGroup, QUOTE_INDEX);
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      double assetWeight = 1;
      if (healthType == HealthType::Maint) {
        assetWeight = mangoGroup.spotMarkets[i].maintAssetWeight.toDouble();
      } else if (healthType == HealthType::Init) {
        assetWeight = mangoGroup.spotMarkets[i].initAssetWeight.toDouble();
      }
      auto spotVal = getSpotVal(mangoGroup, mangoCache, i, assetWeight);
      assetsVal += spotVal;

      const auto price = mangoCache.price_cache[i].price.toDouble();
      auto perpAssetVal = getPerpAccountAssetVal(
          mangoAccountInfo.perpAccounts[i], mangoGroup.perpMarkets[i], price,
          mangoCache.perp_market_cache[i].short_funding.toDouble(),
          mangoCache.perp_market_cache[i].long_funding.toDouble());
      const auto perpsUiAssetVal = nativeI80F48ToUi(
          perpAssetVal, mangoGroup.tokens[QUOTE_INDEX].decimals);
      assetsVal += perpsUiAssetVal;
    }
    return assetsVal;
  }
  double getLiabsVal(const MangoGroup& mangoGroup, const MangoCache& mangoCache,
                     HealthType healthType) {
    double liabsVal = 0;
    liabsVal += getUiBorrow(mangoCache.root_bank_cache[QUOTE_INDEX], mangoGroup,
                            QUOTE_INDEX);
    for (uint64_t i = 0; i < mangoGroup.numOracles; ++i) {
      double liabWeight = 1;
      auto price = getMangoGroupPrice(mangoGroup, i, mangoCache);
      if (healthType == HealthType::Maint) {
        liabWeight = mangoGroup.spotMarkets[i].maintLiabWeight.toDouble();
      } else if (healthType == HealthType::Init) {
        liabWeight = mangoGroup.spotMarkets[i].initLiabWeight.toDouble();
      }
      liabsVal += getUiBorrow(mangoCache.root_bank_cache[i], mangoGroup, i) *
                  (price * liabWeight);
      const auto perpsUiLiabsVal = nativeI80F48ToUi(
          getPerpAccountLiabsVal(
              mangoAccountInfo.perpAccounts[i], mangoGroup.perpMarkets[i],
              mangoCache.price_cache[i].price.toDouble(),
              mangoCache.perp_market_cache[i].short_funding.toDouble(),
              mangoCache.perp_market_cache[i].long_funding.toDouble()),
          mangoGroup.tokens[QUOTE_INDEX].decimals);
      liabsVal += perpsUiLiabsVal;
    }
    return liabsVal;
  }
  double getUiBorrow(const RootBankCache& rootBankCache,
                     const MangoGroup& mangoGroup, uint64_t tokenIndex) {
    return nativeI80F48ToUi(
        ceil(getNativeBorrow(rootBankCache, mangoGroup, tokenIndex)),
        getMangoGroupTokenDecimals(mangoGroup, tokenIndex));
  }
  double getNativeBorrow(const RootBankCache& rootBankCache,
                         const MangoGroup& mangoGroup, uint64_t tokenIndex) {
    return rootBankCache.borrow_index.toDouble() *
           mangoAccountInfo.borrows[tokenIndex].toDouble();
  }
  double getNativeDeposit(const RootBankCache& rootBankCache,
                          uint64_t tokenIndex) {
    return rootBankCache.deposit_index.toDouble() *
           mangoAccountInfo.deposits[tokenIndex].toDouble();
  }
  double getUiDeposit(const RootBankCache& rootBankCache,
                      const MangoGroup& mangoGroup, uint64_t tokenIndex) {
    auto result =
        nativeI80F48ToUi(floor(getNativeDeposit(rootBankCache, tokenIndex)),
                         getMangoGroupTokenDecimals(mangoGroup, tokenIndex));
    return result;
  }
  double getSpotVal(const MangoGroup& mangoGroup, const MangoCache& mangoCache,
                    uint64_t index, double assetWeight) {
    double assetsVal = 0;
    auto price = getMangoGroupPrice(mangoGroup, index, mangoCache);
    auto depositVal =
        getUiDeposit(mangoCache.root_bank_cache[index], mangoGroup, index) *
        price * assetWeight;
    assetsVal += depositVal;
    try {
      const auto openOrdersAccount = spotOpenOrdersAccounts.at(
          mangoAccountInfo.spotOpenOrders[index].toBase58());
      assetsVal += nativeToUi(openOrdersAccount.baseTokenTotal,
                              mangoGroup.tokens[index].decimals) *
                   price * assetWeight;
      assetsVal += nativeToUi(openOrdersAccount.quoteTokenTotal +
                                  openOrdersAccount.referrerRebatesAccrued,
                              mangoGroup.tokens[QUOTE_INDEX].decimals);
    } catch (std::out_of_range& e) {
      return assetsVal;
    }
    return assetsVal;
  }
};

}  // namespace mango_v3