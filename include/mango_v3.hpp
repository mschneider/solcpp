#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "fixedp.h"
#include "int128.hpp"
#include "serum_v3.hpp"
#include "solana.hpp"

namespace mango_v3 {
using json = nlohmann::json;

const int MAX_TOKENS = 16;
const int MAX_PAIRS = 15;
const int MAX_PERP_OPEN_ORDERS = 64;
const int INFO_LEN = 32;
const int QUOTE_INDEX = 15;
const int EVENT_SIZE = 200;
const int EVENT_QUEUE_SIZE = 256;

struct Config {
  std::string endpoint;
  std::string group;
  std::string program;
  std::vector<uint8_t> decimals;
  std::vector<std::string> symbols;
};

const Config MAINNET = {
    "https://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88",
    "98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue",
    "mv3ekLzLbnVPNxjSKvqBpU3ZeZXPQdEC3bp5MDEBG68",
    {6, 6, 6, 9, 6, 6, 6, 6, 6, 9, 8, 8, 6, 0, 0, 6},
    {"MNGO", "BTC", "ETH", "SOL", "USDT", "SRM", "RAY", "COPE", "FTT", "MSOL",
     "BNB", "AVAX", "LUNA", "", "", "USDC"}};

const Config DEVNET = {
    "https://mango.devnet.rpcpool.com",
    "Ec2enZyoC4nGpEfu2sUNAa2nUGJHWxoUWYSEJ2hNTWTA",
    "4skJ85cdxQAFVKbcGgfun8iZPL7BadVYXG3kGEGkufqA",
    {6, 6, 6, 9, 6, 6, 6, 6, 6, 9, 8, 8, 8, 0, 0, 6},
    {"MNGO", "BTC", "ETH", "SOL", "SRM", "RAY", "USDT", "ADA", "FTT", "AVAX",
     "LUNA", "BNB", "MATIC", "", "", "USDC"}};

enum HealthType { Unknown, Init, Maint };

// all rust structs assume padding to 8
#pragma pack(push, 8)

struct MetaData {
  uint8_t dataType;
  uint8_t version;
  uint8_t isInitialized;
  uint8_t padding[5];
};

struct TokenInfo {
  solana::PublicKey mint;
  solana::PublicKey rootBank;
  uint8_t decimals;
  uint8_t padding[7];
};

struct SpotMarketInfo {
  solana::PublicKey spotMarket;
  i80f48 maintAssetWeight;
  i80f48 initAssetWeight;
  i80f48 maintLiabWeight;
  i80f48 initLiabWeight;
  i80f48 liquidationFee;
};

struct PerpMarketInfo {
  solana::PublicKey perpMarket;
  i80f48 maintAssetWeight;
  i80f48 initAssetWeight;
  i80f48 maintLiabWeight;
  i80f48 initLiabWeight;
  i80f48 liquidationFee;
  i80f48 makerFee;
  i80f48 takerFee;
  int64_t baseLotSize;
  int64_t quoteLotSize;
};

struct MangoGroup {
  MetaData metaData;
  uint64_t numOracles;
  TokenInfo tokens[MAX_TOKENS];
  SpotMarketInfo spotMarkets[MAX_PAIRS];
  PerpMarketInfo perpMarkets[MAX_PAIRS];
  solana::PublicKey oracles[MAX_PAIRS];
  uint64_t signerNonce;
  solana::PublicKey signerKey;
  solana::PublicKey admin;
  solana::PublicKey dexProgramId;
  solana::PublicKey mangoCache;
  uint64_t validInterval;
  solana::PublicKey insuranceVault;
  solana::PublicKey srmVault;
  solana::PublicKey msrmVault;
  solana::PublicKey feesVault;
  uint32_t maxMangoAccounts;
  uint32_t numMangoAccounts;
  uint8_t padding[24];
};
struct RootBankCache {
  i80f48 deposit_index;
  i80f48 borrow_index;
  uint64_t last_update;
};
struct PerpMarketCache {
  i80f48 long_funding;
  i80f48 short_funding;
  uint64_t last_update;
};
struct PriceCache {
  i80f48 price;
  uint64_t last_update;
};
struct MangoCache {
  MetaData metadata;
  PriceCache price_cache[MAX_PAIRS];
  RootBankCache root_bank_cache[MAX_TOKENS];
  PerpMarketCache perp_market_cache[MAX_PAIRS];
};

enum Side : uint8_t { Buy, Sell };

struct PerpAccountInfo {
  int64_t basePosition;
  i80f48 quotePosition;
  i80f48 longSettledFunding;
  i80f48 shortSettledFunding;
  int64_t bidsQuantity;
  int64_t asksQuantity;
  int64_t takerBase;
  int64_t takerQuote;
  uint64_t mngoAccrued;
};

struct MangoAccountInfo {
  MetaData metaData;
  solana::PublicKey mangoGroup;
  solana::PublicKey owner;
  bool inMarginBasket[MAX_PAIRS];
  uint8_t numInMarginBasket;
  i80f48 deposits[MAX_TOKENS];
  i80f48 borrows[MAX_TOKENS];
  solana::PublicKey spotOpenOrders[MAX_PAIRS];
  PerpAccountInfo perpAccounts[MAX_PAIRS];
  uint8_t orderMarket[MAX_PERP_OPEN_ORDERS];
  Side orderSide[MAX_PERP_OPEN_ORDERS];
  __int128_t orders[MAX_PERP_OPEN_ORDERS];
  uint64_t clientOrderIds[MAX_PERP_OPEN_ORDERS];
  uint64_t msrmAmount;
  bool beingLiquidated;
  bool isBankrupt;
  uint8_t info[INFO_LEN];
  solana::PublicKey advancedOrdersKey;
  bool notUpgradable;
  solana::PublicKey delegate;
  uint8_t padding[5];
};
namespace {
MangoCache loadCache(solana::rpc::Connection& connection,
                     const std::string& mangoCachePubKey) {
  auto account = connection.getAccountInfo<MangoCache>(mangoCachePubKey);
  return std::move(account);
}
// quoteFree, quoteLocked, baseFree, baseLocked
std::tuple<i80f48, i80f48, i80f48, i80f48> splitOpenOrders(
    const serum_v3::OpenOrders* openOrders) {
  const auto quoteFree =
      i80f48((unsigned int)(openOrders->quoteTokenFree +
                            openOrders->referrerRebatesAccrued));
  const auto quoteLocked = i80f48(
      (unsigned int)(openOrders->quoteTokenTotal - openOrders->quoteTokenFree));
  const auto baseFree = i80f48((unsigned int)(openOrders->baseTokenFree));
  const auto baseLocked = i80f48(
      (unsigned int)(openOrders->baseTokenTotal - openOrders->baseTokenFree));
  return std::make_tuple(quoteFree, quoteLocked, baseFree, baseLocked);
}
i80f48 getUnsettledFunding(const PerpAccountInfo* accountInfo,
                           const PerpMarketCache* perpMarketCache) {
  if (accountInfo->basePosition < 0) {
    return i80f48((unsigned int)accountInfo->basePosition) *
           (perpMarketCache->short_funding - accountInfo->shortSettledFunding);
  } else {
    return i80f48((unsigned int)accountInfo->basePosition) *
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
    HealthType healthType = Unknown) {
  if (healthType == Maint) {
    return std::make_tuple(
        mangoGroup->spotMarkets[marketIndex].maintAssetWeight,
        mangoGroup->spotMarkets[marketIndex].maintLiabWeight,
        mangoGroup->perpMarkets[marketIndex].maintAssetWeight,
        mangoGroup->perpMarkets[marketIndex].maintLiabWeight);
  } else if (healthType == Init) {
    return std::make_tuple(mangoGroup->spotMarkets[marketIndex].initAssetWeight,
                           mangoGroup->spotMarkets[marketIndex].initLiabWeight,
                           mangoGroup->perpMarkets[marketIndex].initAssetWeight,
                           mangoGroup->perpMarkets[marketIndex].initLiabWeight);
  } else {
    return std::make_tuple(1, 1, 1, 1);
  }
}
}  // namespace
struct MangoAccount {
  MangoAccountInfo accountInfo;
  std::map<std::string, serum_v3::OpenOrders>
      spotOpenOrdersAccounts;  // Address, AccountInfo
  explicit MangoAccount(const MangoAccountInfo& accountInfo_) noexcept {
    accountInfo = accountInfo_;
  }
  // Fetch `accountInfo` from `endpoint` and decode it
  explicit MangoAccount(const std::string& pubKey,
                        const std::string& endpoint = MAINNET.endpoint) {
    auto connection = solana::rpc::Connection(endpoint);
    const auto& accountInfo_ =
        connection.getAccountInfo<MangoAccountInfo>(pubKey);
    accountInfo = accountInfo_;
  }
  std::map<std::string, serum_v3::OpenOrders> loadOpenOrders(
      solana::rpc::Connection& connection);
  // Return the spot, perps and quote currency values after adjusting for
  // worst case open orders scenarios. These values are not adjusted for health
  // type
  // TODO: Change `spot` and `perps` from vec to array for use in `getHealth()`
  std::tuple<std::vector<i80f48>, std::vector<i80f48>, i80f48>
  getHealthComponents(MangoGroup* mangoGroup, MangoCache* mangoCache);
  i80f48 getHealthFromComponents(MangoGroup* mangoGroup, MangoCache* mangoCache,
                                 std::vector<i80f48> spot,
                                 std::vector<i80f48> perps, i80f48 quote,
                                 HealthType healthType);
  // deposits - borrows in native terms
  i80f48 getNet(RootBankCache rootBankCache, size_t tokenIndex) {
    return accountInfo.deposits[tokenIndex] * rootBankCache.deposit_index -
           (accountInfo.borrows[tokenIndex] * rootBankCache.borrow_index);
  }
  i80f48 getHealth(MangoGroup* mangoGroup, MangoCache* mangoCache,
                   HealthType healthType);
  // Take health components and return the assets and liabs weighted
  std::pair<i80f48, i80f48> getWeightedAssetsLiabsVals(
      MangoGroup* mangoGroup, MangoCache* mangoCache, std::vector<i80f48> spot,
      std::vector<i80f48> perps, i80f48 quote, HealthType healthType = Unknown);
  i80f48 getHealthRatio(MangoGroup* mangoGroup, MangoCache* mangoCache,
                        HealthType healthType = Unknown);
};
std::map<std::string, serum_v3::OpenOrders> MangoAccount::loadOpenOrders(
    solana::rpc::Connection& connection) {
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
        // Check initialized and OpenOrders account flags
        return !((accountInfo.second.accountFlags &
                  serum_v3::AccountFlags::Initialized) !=
                     serum_v3::AccountFlags::Initialized ||
                 (accountInfo.second.accountFlags &
                  serum_v3::AccountFlags::OpenOrders) !=
                     serum_v3::AccountFlags::OpenOrders);
      });
  return spotOpenOrdersAccounts;
}

std::tuple<std::vector<i80f48>, std::vector<i80f48>, i80f48>
MangoAccount::getHealthComponents(MangoGroup* mangoGroup,
                                  MangoCache* mangoCache) {
  std::vector<i80f48> spot(mangoGroup->numOracles, i80f48(0));
  std::vector<i80f48> perps(mangoGroup->numOracles, i80f48(0));
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
        quote = quote + (baseLocked * price) + quoteFree + quoteLocked;
      }
    } else {
      spot[i] = baseNet;
    }
    // Evaluate perps
    if (!mangoGroup->perpMarkets[i].perpMarket.toBase58().empty()) {
      const auto perpMarketCache = mangoCache->perp_market_cache[i];
      const auto perpAccount = accountInfo.perpAccounts[i];
      const auto baseLotSize = mangoGroup->perpMarkets[i].baseLotSize;
      const auto quoteLotSize = mangoGroup->perpMarkets[i].quoteLotSize;
      const auto takerQuote =
          i80f48(((unsigned int)(perpAccount.takerQuote * quoteLotSize)));
      const auto basePos = i80f48(
          (unsigned int)((perpAccount.basePosition + perpAccount.takerBase) *
                         baseLotSize));
      auto bidsQuantity =
          i80f48((unsigned int)(perpAccount.bidsQuantity * baseLotSize));
      auto asksQuantity =
          i80f48((unsigned int)(perpAccount.asksQuantity * baseLotSize));
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
      perps[i] = i80f48(0);
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
        (spot[i] > i80f48(0) ? spotAssetWeight : spotLiabWeight);
    const auto perpHealth =
        (perps[i] * price) *
        (perps[i] > i80f48(0) ? perpAssetWeight : perpLiabWeight);
    health += spotHealth;
    health += perpHealth;
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
  auto assets = i80f48(0);
  auto liabs = i80f48(0);
  if (quote > 0) {
    assets = assets + quote;
  } else {
    liabs = liabs + (quote * i80f48(-1));
  }
  for (int i = 0; i < mangoGroup->numOracles; i++) {
    const auto [spotAssetWeight, spotLiabWeight, perpAssetWeight,
                perpLiabWeight] = getWeights(mangoGroup, i, healthType);
    const auto price = mangoCache->price_cache[i].price;
    if (spot[i] > 0) {
      assets = (spot[i] * price * spotAssetWeight) + assets;
    } else {
      liabs = ((spot[i] * i80f48(-1)) * price * spotLiabWeight) + liabs;
    }

    if (perps[i] > 0) {
      assets = (perps[i] * price * perpAssetWeight) + assets;
    } else {
      liabs = ((perps[i] * i80f48(-1)) * price * perpLiabWeight) + liabs;
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
  if (liabs > i80f48(0)) {
    return ((assets / liabs) - i80f48(1)) * i80f48(100);
  } else {
    return i80f48(100);
  }
}

struct LiquidityMiningInfo {
  i80f48 rate;
  i80f48 maxDepthBps;
  uint64_t periodStart;
  uint64_t targetPeriodLength;
  uint64_t mngoLeft;
  uint64_t mngoPerPeriod;
};

struct PerpMarket {
  MetaData metaData;
  solana::PublicKey mangoGroup;
  solana::PublicKey bids;
  solana::PublicKey asks;
  solana::PublicKey eventQueue;
  int64_t quoteLotSize;
  int64_t baseLotSize;
  i80f48 longFunding;
  i80f48 shortFunding;
  int64_t openInterest;
  uint64_t lastUpdated;
  uint64_t seqNum;
  i80f48 feesAccrued;
  LiquidityMiningInfo liquidityMiningInfo;
  solana::PublicKey mngoVault;
};

struct EventQueueHeader {
  MetaData metaData;
  uint64_t head;
  uint64_t count;
  uint64_t seqNum;
};

enum EventType : uint8_t { Fill, Out, Liquidate };

struct AnyEvent {
  EventType eventType;
  uint8_t padding[EVENT_SIZE - 1];
};

struct FillEvent {
  EventType eventType;
  Side takerSide;
  uint8_t makerSlot;
  uint8_t makerOut;
  uint8_t version;
  uint8_t padding[3];
  uint64_t timestamp;
  uint64_t seqNum;
  solana::PublicKey maker;
  __int128_t makerOrderId;
  uint64_t makerClientOrderId;
  i80f48 makerFee;
  int64_t bestInitial;
  uint64_t makerTimestamp;
  solana::PublicKey taker;
  __int128_t takerOrderId;
  uint64_t takerClientOrderId;
  i80f48 takerFee;
  int64_t price;
  int64_t quantity;
};

struct LiquidateEvent {
  EventType eventType;
  uint8_t padding0[7];
  uint64_t timestamp;
  uint64_t seqNum;
  solana::PublicKey liqee;
  solana::PublicKey liqor;
  i80f48 price;
  int64_t quantity;
  i80f48 liquidationFee;
  uint8_t padding1[EVENT_SIZE - 128];
};

struct OutEvent {
  EventType eventType;
  Side side;
  uint8_t slot;
  uint8_t padding0[5];
  uint64_t timestamp;
  uint64_t seqNum;
  solana::PublicKey owner;
  int64_t quantity;
  uint8_t padding1[EVENT_SIZE - 64];
};

struct EventQueue {
  EventQueueHeader header;
  AnyEvent items[EVENT_QUEUE_SIZE];
};

#pragma pack(pop)

// instructions are even tighter packed, every byte counts
#pragma pack(push, 1)

namespace ix {
template <typename T>
std::vector<uint8_t> toBytes(const T& ref) {
  const auto bytePtr = (uint8_t*)&ref;
  return std::vector<uint8_t>(bytePtr, bytePtr + sizeof(T));
}

std::pair<int64_t, int64_t> uiToNativePriceQuantity(double price,
                                                    double quantity,
                                                    const Config& config,
                                                    const int marketIndex,
                                                    const PerpMarket& market) {
  const int64_t baseUnit = pow(10LL, config.decimals[marketIndex]);
  const int64_t quoteUnit = pow(10LL, config.decimals[QUOTE_INDEX]);
  const auto nativePrice = ((int64_t)(price * quoteUnit)) * market.baseLotSize /
                           (market.quoteLotSize * baseUnit);
  const auto nativeQuantity =
      ((int64_t)(quantity * baseUnit)) / market.baseLotSize;
  return std::pair<int64_t, int64_t>(nativePrice, nativeQuantity);
};

enum OrderType : uint8_t {
  Limit = 0,
  IOC = 1,
  PostOnly = 2,
  Market = 3,
  PostOnlySlide = 4
};

struct PlacePerpOrder {
  static const uint32_t CODE = 12;
  uint32_t ixs = CODE;
  int64_t price;
  int64_t quantity;
  uint64_t clientOrderId;
  Side side;
  OrderType orderType;
  uint8_t reduceOnly;
};

solana::Instruction placePerpOrderInstruction(
    const PlacePerpOrder& ixData, const solana::PublicKey& ownerPk,
    const solana::PublicKey& accountPk, const solana::PublicKey& marketPk,
    const PerpMarket& market, const solana::PublicKey& groupPk,
    const MangoGroup& group, const solana::PublicKey& programPk) {
  std::vector<solana::AccountMeta> accs = {
      {groupPk, false, false},    {accountPk, false, true},
      {ownerPk, true, false},     {group.mangoCache, false, false},
      {marketPk, false, true},    {market.bids, false, true},
      {market.asks, false, true}, {market.eventQueue, false, true},
  };
  for (int i = 0; i < MAX_PAIRS; ++i) {
    accs.push_back({{}, false, false});
  }
  return {programPk, accs, toBytes(ixData)};
};

struct CancelAllPerpOrders {
  static const uint32_t CODE = 39;
  uint32_t ixs = CODE;
  // limit the number of cancelled orders to stay within compute limits
  uint8_t limit;
};

solana::Instruction cancelAllPerpOrdersInstruction(
    const CancelAllPerpOrders& ixData, const solana::PublicKey& ownerPk,
    const solana::PublicKey& accountPk, const solana::PublicKey& marketPk,
    const PerpMarket& market, const solana::PublicKey& groupPk,
    const solana::PublicKey& programPk) {
  const std::vector<solana::AccountMeta> accs = {
      {groupPk, false, false},    {accountPk, false, true},
      {ownerPk, true, false},     {marketPk, false, true},
      {market.bids, false, true}, {market.asks, false, true},
  };
  return {programPk, accs, toBytes(ixData)};
};
}  // namespace ix

#pragma pack(pop)

}  // namespace mango_v3
