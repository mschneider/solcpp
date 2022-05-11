#pragma once
#include <fmt/format.h>

#include "mango_v3.hpp"

namespace mango_v3 {
// quoteFree, quoteLocked, baseFree, baseLocked
auto splitOpenOrders(const serum_v3::OpenOrders& openOrders) {
  const auto quoteFree =
      openOrders.quoteTokenFree + openOrders.referrerRebatesAccrued;
  const auto quoteLocked =
      (openOrders.quoteTokenTotal - openOrders.quoteTokenFree);
  const auto baseFree = openOrders.baseTokenFree;
  const auto baseLocked = openOrders.baseTokenTotal - openOrders.baseTokenFree;
  return std::make_tuple(quoteFree, quoteLocked, baseFree, baseLocked);
}
auto getUnsettledFunding(const PerpAccountInfo& accountInfo,
                         const PerpMarketCache& perpMarketCache) {
  if (accountInfo.basePosition < 0) {
    return accountInfo.basePosition *
           (perpMarketCache.short_funding - accountInfo.shortSettledFunding);
  } else {
    return accountInfo.basePosition *
           (perpMarketCache.long_funding - accountInfo.longSettledFunding);
  }
}
// Return the quote position after adjusting for unsettled funding
auto getQuotePosition(const PerpAccountInfo& accountInfo,
                      const PerpMarketCache& perpMarketCache) {
  return accountInfo.quotePosition -
         getUnsettledFunding(accountInfo, perpMarketCache);
}
/**
 * Return weights corresponding to health type;
 * Weights are all 1 if no healthType provided
 * @return
 * <spotAssetWeight, spotLiabWeight,perpAssetWeight, perpLiabWeight>
 */
auto getMangoGroupWeights(const MangoGroup& mangoGroup, uint64_t marketIndex,
                          HealthType healthType = HealthType::Unknown) {
  if (healthType == HealthType::Maint) {
    return std::make_tuple(mangoGroup.spotMarkets[marketIndex].maintAssetWeight,
                           mangoGroup.spotMarkets[marketIndex].maintLiabWeight,
                           mangoGroup.perpMarkets[marketIndex].maintAssetWeight,
                           mangoGroup.perpMarkets[marketIndex].maintLiabWeight);
  } else if (healthType == HealthType::Init) {
    return std::make_tuple(mangoGroup.spotMarkets[marketIndex].initAssetWeight,
                           mangoGroup.spotMarkets[marketIndex].initLiabWeight,
                           mangoGroup.perpMarkets[marketIndex].initAssetWeight,
                           mangoGroup.perpMarkets[marketIndex].initLiabWeight);
  } else {
    return std::make_tuple(i80f48(1.0), i80f48(1.0), i80f48(1.0), i80f48(1.0));
  }
}
auto nativeI80F48ToUi(i80f48 amount, uint8_t decimals) {
  return amount / pow(10, decimals);
}
auto getPerpAccountAssetVal(const PerpAccountInfo& perpAccountInfo,
                            const PerpMarketInfo& perpMarketInfo, i80f48 price,
                            i80f48 shortFunding, i80f48 longFunding) {
  i80f48 assetsVal = 0.0;
  if (perpAccountInfo.basePosition > 0) {
    assetsVal +=
        (perpAccountInfo.basePosition * perpMarketInfo.baseLotSize * price);
  }
  auto realQuotePosition = perpAccountInfo.quotePosition;
  if (perpAccountInfo.basePosition > 0) {
    realQuotePosition = perpAccountInfo.quotePosition -
                        ((longFunding - perpAccountInfo.longSettledFunding) *
                         perpAccountInfo.basePosition);
  } else if (perpAccountInfo.basePosition < 0) {
    realQuotePosition = perpAccountInfo.quotePosition -
                        ((shortFunding - perpAccountInfo.shortSettledFunding) *
                         perpAccountInfo.basePosition);
  }

  if (realQuotePosition > 0) {
    assetsVal += realQuotePosition;
  }
  return assetsVal;
}
auto getPerpAccountLiabsVal(const PerpAccountInfo& perpAccountInfo,
                            const PerpMarketInfo& perpMarketInfo, i80f48 price,
                            i80f48 shortFunding, i80f48 longFunding) {
  i80f48 liabsVal = 0.0;
  if (perpAccountInfo.basePosition < 0) {
    liabsVal +=
        (perpAccountInfo.basePosition * perpMarketInfo.baseLotSize * price);
  }
  auto realQuotePosition = perpAccountInfo.quotePosition;
  if (perpAccountInfo.basePosition > 0) {
    realQuotePosition = perpAccountInfo.quotePosition -
                        ((longFunding - perpAccountInfo.longSettledFunding) *
                         perpAccountInfo.basePosition);
  } else if (perpAccountInfo.basePosition < 0) {
    realQuotePosition = perpAccountInfo.quotePosition -
                        ((shortFunding - perpAccountInfo.shortSettledFunding) *
                         perpAccountInfo.basePosition);
  }
  if (realQuotePosition < 0) {
    liabsVal += realQuotePosition;
  }
  return liabsVal * -1;
}
/**
 * Return the decimals in TokenInfo;
 * If it's not QUOTE_INDEX and there is an oracle for this index but no
 * SPL-Token, this will default to 6 Otherwise throw error
 */
uint8_t getMangoGroupTokenDecimals(const MangoGroup& mangoGroup,
                                   uint64_t tokenIndex) {
  const auto tokenInfo = mangoGroup.tokens[tokenIndex];
  if (tokenInfo.decimals == 0) {
    if (mangoGroup.oracles[tokenIndex] == solana::PublicKey::empty()) {
      throw std::runtime_error(
          fmt::format("No oracle for this tokenIndex: {}", tokenIndex));
    } else {
      return 6;
    }
  } else {
    return tokenInfo.decimals;
  }
}
i80f48 getMangoGroupPrice(const MangoGroup& mangoGroup, uint64_t tokenIndex,
                          const MangoCache& mangoCache) {
  if (tokenIndex == QUOTE_INDEX) {
    return 1;
  }
  const auto decimalAdj =
      pow(10, getMangoGroupTokenDecimals(mangoGroup, tokenIndex) -
                  getMangoGroupTokenDecimals(mangoGroup, QUOTE_INDEX));
  return mangoCache.price_cache[tokenIndex].price * decimalAdj;
}
double nativeToUi(uint64_t amount, uint8_t decimals) {
  return amount / pow(10, decimals);
}
}  // namespace mango_v3