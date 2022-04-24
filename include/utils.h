#pragma once
#include "mango_v3.hpp"

namespace mango_v3 {
// quoteFree, quoteLocked, baseFree, baseLocked
std::tuple<i80f48, i80f48, i80f48, i80f48> splitOpenOrders(
    const serum_v3::OpenOrders* openOrders) {
  const auto quoteFree =
      i80f48((openOrders->quoteTokenFree + openOrders->referrerRebatesAccrued));
  const auto quoteLocked =
      i80f48((openOrders->quoteTokenTotal - openOrders->quoteTokenFree));
  const auto baseFree = i80f48((openOrders->baseTokenFree));
  const auto baseLocked =
      i80f48((openOrders->baseTokenTotal - openOrders->baseTokenFree));
  return std::make_tuple(quoteFree, quoteLocked, baseFree, baseLocked);
}
i80f48 getUnsettledFunding(const PerpAccountInfo* accountInfo,
                           const PerpMarketCache* perpMarketCache) {
  if (accountInfo->basePosition < 0) {
    return i80f48(accountInfo->basePosition) *
           (perpMarketCache->short_funding - accountInfo->shortSettledFunding);
  } else {
    return i80f48(accountInfo->basePosition) *
           (perpMarketCache->long_funding - accountInfo->longSettledFunding);
  }
}
// Return the quote position after adjusting for unsettled funding
i80f48 getQuotePosition(const PerpAccountInfo* accountInfo,
                        const PerpMarketCache* perpMarketCache) {
  return accountInfo->quotePosition -
         getUnsettledFunding(accountInfo, perpMarketCache);
}
/**
 * Return weights corresponding to health type;
 * Weights are all 1 if no healthType provided
 * @return
 * <spotAssetWeight, spotLiabWeight,perpAssetWeight, perpLiabWeight>
 */
std::tuple<i80f48, i80f48, i80f48, i80f48> getWeights(
    MangoGroup* mangoGroup, size_t marketIndex,
    HealthType healthType = HealthType::Unknown) {
  if (healthType == HealthType::Maint) {
    return std::make_tuple(
        mangoGroup->spotMarkets[marketIndex].maintAssetWeight,
        mangoGroup->spotMarkets[marketIndex].maintLiabWeight,
        mangoGroup->perpMarkets[marketIndex].maintAssetWeight,
        mangoGroup->perpMarkets[marketIndex].maintLiabWeight);
  } else if (healthType == HealthType::Init) {
    return std::make_tuple(mangoGroup->spotMarkets[marketIndex].initAssetWeight,
                           mangoGroup->spotMarkets[marketIndex].initLiabWeight,
                           mangoGroup->perpMarkets[marketIndex].initAssetWeight,
                           mangoGroup->perpMarkets[marketIndex].initLiabWeight);
  } else {
    return std::make_tuple(i80f48(1L), i80f48(1L), i80f48(1L), i80f48(1L));
  }
}
i80f48 nativeI80F48ToUi(i80f48 amount, uint8_t decimals) {
  return amount / i80f48(std::pow(10, decimals));
}
i80f48 getAssetVal(PerpAccountInfo* perpAccountInfo,
                   PerpMarketInfo* perpMarketInfo, i80f48 price,
                   i80f48 shortFunding, i80f48 longFunding) {
  auto assetsVal = i80f48(0ULL);
  if (perpAccountInfo->basePosition > 0) {
    assetsVal +=
        (i80f48(perpAccountInfo->basePosition * perpMarketInfo->baseLotSize) *
         price);
  }
  auto realQuotePosition = perpAccountInfo->quotePosition;
  if (perpAccountInfo->basePosition > 0) {
    realQuotePosition = perpAccountInfo->quotePosition -
                        ((longFunding - perpAccountInfo->longSettledFunding) *
                         i80f48(perpAccountInfo->basePosition));
  } else if (perpAccountInfo->basePosition < 0) {
    realQuotePosition = perpAccountInfo->quotePosition -
                        ((shortFunding - perpAccountInfo->shortSettledFunding) *
                         i80f48(perpAccountInfo->basePosition));
  }

  if (realQuotePosition > i80f48(0ULL)) {
    assetsVal += realQuotePosition;
  }
  return assetsVal;
}
i80f48 getLiabsVal(PerpAccountInfo* perpAccountInfo,
                   PerpMarketInfo* perpMarketInfo, i80f48 price,
                   i80f48 shortFunding, i80f48 longFunding) {
  auto liabsVal = i80f48(0ULL);
  if (perpAccountInfo->basePosition < 0) {
    liabsVal +=
        (i80f48(perpAccountInfo->basePosition * perpMarketInfo->baseLotSize) *
         price);
  }
  auto realQuotePosition = perpAccountInfo->quotePosition;
  if (perpAccountInfo->basePosition > 0) {
    realQuotePosition = perpAccountInfo->quotePosition -
                        ((longFunding - perpAccountInfo->longSettledFunding) *
                         i80f48(perpAccountInfo->basePosition));
  } else if (perpAccountInfo->basePosition < 0) {
    realQuotePosition = perpAccountInfo->quotePosition -
                        ((shortFunding - perpAccountInfo->shortSettledFunding) *
                         i80f48(perpAccountInfo->basePosition));
  }
  if (realQuotePosition < i80f48(0ULL)) {
    liabsVal += realQuotePosition;
  }
  return liabsVal * i80f48(-1ULL);
}
}  // namespace mango_v3