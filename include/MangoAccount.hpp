#pragma once

#include "mango_v3.hpp"
#include "solana.hpp"
#include "utils.hpp"

namespace mango_v3 {
struct MangoAccount {
  MangoAccountInfo mangoAccountInfo;
  // {address, OpenOrders}
  std::unordered_map<std::string, serum_v3::OpenOrders> spotOpenOrdersAccounts;
  explicit MangoAccount(const MangoAccountInfo& accountInfo_) noexcept {
    mangoAccountInfo = accountInfo_;
    // TODO: Call `loadOpenOrders()`
  }
  explicit MangoAccount(const solana::PublicKey& pubKey,
                        solana::rpc::Connection& connection) {
    auto accountInfo_ =
        connection.getAccountInfo<MangoAccountInfo>(pubKey).value.data;
    mangoAccountInfo = accountInfo_;
  }
  // Returns map(Address: OpenOrders) and sets this accounts
  // `spotOpenOrdersAccounts`
  auto loadOpenOrders(solana::rpc::Connection& connection) {
    // Filter only non-empty open orders
    std::vector<solana::PublicKey> filteredOpenOrders;
    for (auto item : mangoAccountInfo.spotOpenOrders) {
      if (item == solana::PublicKey::empty()) continue;
      filteredOpenOrders.emplace_back(item);
    }
    const auto accountsInfo =
        connection
            .getMultipleAccountsInfo<serum_v3::OpenOrders>(filteredOpenOrders)
            .value;
    // clear spotOpenOrdersAccounts
    spotOpenOrdersAccounts.clear();
    // take out data from accountInfo
    std::vector<serum_v3::OpenOrders> accountsInfoData;
    auto index = 0;
    for (const auto& accountInfo : accountsInfo) {
        spotOpenOrdersAccounts[filteredOpenOrders[index].toBase58()] = accountInfo.value().data;
    }
    return spotOpenOrdersAccounts;
  }

  /**
   * deposits - borrows in native terms
   */
  i80f48 getNet(const RootBankCache& cache, uint64_t tokenIndex) {
    return (mangoAccountInfo.deposits[tokenIndex] * cache.deposit_index) -
           (mangoAccountInfo.borrows[tokenIndex] * cache.borrow_index);
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
    std::vector<i80f48> spot(mangoGroup.numOracles, 0.0L);
    std::vector<i80f48> perps(mangoGroup.numOracles, 0.0L);
    auto quote = getNet(mangoCache.root_bank_cache[QUOTE_INDEX], QUOTE_INDEX);
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      const auto bankCache = mangoCache.root_bank_cache[i];
      const auto price = mangoCache.price_cache[i].price;
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
        if (abs(bidsBaseNet.to_double()) > abs(asksBaseNet.to_double())) {
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
        perps[i] = 0L;
      }
    }
    return std::make_tuple(spot, perps, quote);
  }
  static auto getHealthFromComponents(const MangoGroup& mangoGroup,
                                      const MangoCache& mangoCache,
                                      std::vector<i80f48> spot,
                                      std::vector<i80f48> perps, i80f48 quote,
                                      HealthType healthType) {
    auto health = quote;
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      const auto [spotAssetWeight, spotLiabWeight, perpAssetWeight,
                  perpLiabWeight] =
          getMangoGroupWeights(mangoGroup, i, healthType);
      const auto price = mangoCache.price_cache[i].price;
      const auto spotHealth =
          (spot[i] * price) * (spot[i] > 0 ? spotAssetWeight : spotLiabWeight);
      const auto perpHealth = (perps[i] * price) *
                              (perps[i] > 0 ? perpAssetWeight : perpLiabWeight);
      health += spotHealth;
      health += perpHealth;
      auto a = spot[i];
      a *= price;
      a *= (spot[i] > 0 ? spotAssetWeight : spotLiabWeight);
    }
    return health;
  }
  auto getHealth(const MangoGroup& mangoGroup, const MangoCache& mangoCache,
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
  static auto getWeightedAssetsLiabsVals(const MangoGroup& mangoGroup,
                                         const MangoCache& mangoCache,
                                         std::vector<i80f48> spot,
                                         std::vector<i80f48> perps,
                                         i80f48 quote, HealthType healthType) {
    i80f48 assets = 0.0L;
    i80f48 liabs = 0.0L;
    if (quote > 0) {
      assets += quote;
    } else {
      liabs += (quote * -1);
    }
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      const auto [spotAssetWeight, spotLiabWeight, perpAssetWeight,
                  perpLiabWeight] =
          getMangoGroupWeights(mangoGroup, i, healthType);
      const auto price = mangoCache.price_cache[i].price;
      if (spot[i] > 0) {
        assets += ((spot[i] * price * spotAssetWeight));
      } else {
        liabs += ((spot[i] * -1) * price * spotLiabWeight);
      }

      if (perps[i] > 0) {
        assets += (perps[i] * price * perpAssetWeight);
      } else {
        liabs += (perps[i] * -1 * price * perpLiabWeight);
      }
    }
    return std::make_pair(assets, liabs);
  }
  i80f48 getHealthRatio(const MangoGroup& mangoGroup,
                        const MangoCache& mangoCache, HealthType healthType) {
    auto [spot, perps, quote] = getHealthComponents(mangoGroup, mangoCache);
    auto [assets, liabs] = getWeightedAssetsLiabsVals(
        mangoGroup, mangoCache, spot, perps, quote, healthType);
    if (liabs > 0) {
      return ((assets / liabs) - 1) * 100;
    } else {
      return 100.0L;
    }
  }

  bool isLiquidatable(const MangoGroup& mangoGroup,
                      const MangoCache& mangoCache) {
    auto initHealth = getHealth(mangoGroup, mangoCache, HealthType::Init);
    auto maintHealth = getHealth(mangoGroup, mangoCache, HealthType::Maint);
    return ((mangoAccountInfo.beingLiquidated && initHealth < 0) ||
            (maintHealth < 0));
  }
  i80f48 computeValue(const MangoGroup& mangoGroup,
                      const MangoCache& mangoCache) {
    auto a = getAssetsVal(mangoGroup, mangoCache, HealthType::Unknown);
    auto b = getLiabsVal(mangoGroup, mangoCache, HealthType::Unknown);
    return a - b;
  }
  i80f48 getLeverage(const MangoGroup& mangoGroup,
                     const MangoCache& mangoCache) {
    auto liabs = getLiabsVal(mangoGroup, mangoCache, HealthType::Unknown);
    auto assets = getAssetsVal(mangoGroup, mangoCache, HealthType::Unknown);
    if (assets > 0) {
      return liabs / (assets - liabs);
    }
    return 0;
  }
  i80f48 getAssetsVal(const MangoGroup& mangoGroup,
                      const MangoCache& mangoCache, HealthType healthType) {
    i80f48 assetsVal = 0.0L;
    // quote currency deposits
    assetsVal += getUiDeposit(mangoCache.root_bank_cache[QUOTE_INDEX],
                              mangoGroup, QUOTE_INDEX);
    for (uint64_t i = 0; i < mangoGroup.numOracles; i++) {
      i80f48 assetWeight = 1L;
      if (healthType == HealthType::Maint) {
        assetWeight = mangoGroup.spotMarkets[i].maintAssetWeight;
      } else if (healthType == HealthType::Init) {
        assetWeight = mangoGroup.spotMarkets[i].initAssetWeight;
      }
      auto spotVal = getSpotVal(mangoGroup, mangoCache, i, assetWeight);
      assetsVal += spotVal;

      const auto price = mangoCache.price_cache[i].price;
      auto perpAssetVal = getPerpAccountAssetVal(
          mangoAccountInfo.perpAccounts[i], mangoGroup.perpMarkets[i], price,
          mangoCache.perp_market_cache[i].short_funding,
          mangoCache.perp_market_cache[i].long_funding);
      const auto perpsUiAssetVal = nativeI80F48ToUi(
          perpAssetVal, mangoGroup.tokens[QUOTE_INDEX].decimals);
      assetsVal += perpsUiAssetVal;
    }
    return assetsVal;
  }
  i80f48 getLiabsVal(const MangoGroup& mangoGroup, const MangoCache& mangoCache,
                     HealthType healthType) {
    i80f48 liabsVal = 0L;
    liabsVal += getUiBorrow(mangoCache.root_bank_cache[QUOTE_INDEX], mangoGroup,
                            QUOTE_INDEX);
    for (uint64_t i = 0; i < mangoGroup.numOracles; ++i) {
      i80f48 liabWeight = 1;
      auto price = getMangoGroupPrice(mangoGroup, i, mangoCache);
      if (healthType == HealthType::Maint) {
        liabWeight = mangoGroup.spotMarkets[i].maintLiabWeight;
      } else if (healthType == HealthType::Init) {
        liabWeight = mangoGroup.spotMarkets[i].initLiabWeight;
      }
      liabsVal += getUiBorrow(mangoCache.root_bank_cache[i], mangoGroup, i) *
                  (price * liabWeight);
      const auto perpsUiLiabsVal = nativeI80F48ToUi(
          getPerpAccountLiabsVal(mangoAccountInfo.perpAccounts[i],
                                 mangoGroup.perpMarkets[i],
                                 mangoCache.price_cache[i].price,
                                 mangoCache.perp_market_cache[i].short_funding,
                                 mangoCache.perp_market_cache[i].long_funding),
          mangoGroup.tokens[QUOTE_INDEX].decimals);
      liabsVal += perpsUiLiabsVal;
    }
    return liabsVal;
  }
  i80f48 getUiBorrow(const RootBankCache& rootBankCache,
                     const MangoGroup& mangoGroup, uint64_t tokenIndex) {
    return nativeI80F48ToUi(
        ceil(
            getNativeBorrow(rootBankCache, mangoGroup, tokenIndex).to_double()),
        getMangoGroupTokenDecimals(mangoGroup, tokenIndex));
  }
  i80f48 getNativeBorrow(const RootBankCache& rootBankCache,
                         const MangoGroup& mangoGroup, uint64_t tokenIndex) {
    return rootBankCache.borrow_index * mangoAccountInfo.borrows[tokenIndex];
  }
  i80f48 getNativeDeposit(const RootBankCache& rootBankCache,
                          uint64_t tokenIndex) {
    return rootBankCache.deposit_index * mangoAccountInfo.deposits[tokenIndex];
  }
  i80f48 getUiDeposit(const RootBankCache& rootBankCache,
                      const MangoGroup& mangoGroup, uint64_t tokenIndex) {
    auto result = nativeI80F48ToUi(
        floor(getNativeDeposit(rootBankCache, tokenIndex).to_double()),
        getMangoGroupTokenDecimals(mangoGroup, tokenIndex));
    return result;
  }
  i80f48 getSpotVal(const MangoGroup& mangoGroup, const MangoCache& mangoCache,
                    uint64_t index, i80f48 assetWeight) {
    i80f48 assetsVal = 0L;
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
