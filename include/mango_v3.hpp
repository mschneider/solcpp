#pragma once

#include <cstdint>
#include <string>
#include "fixedp.h"
#include "int128.hpp"
#include "solana.hpp"

namespace mango_v3
{
  using json = nlohmann::json;

  const int MAX_TOKENS = 16;
  const int MAX_PAIRS = 15;
  const int QUOTE_INDEX = 15;
  const int EVENT_SIZE = 200;
  const int EVENT_QUEUE_SIZE = 256;

  struct Config
  {
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
      {"MNGO", "BTC", "ETH", "SOL", "USDT", "SRM", "RAY", "COPE", "FTT", "MSOL", "BNB", "AVAX", "LUNA", "", "", "USDC"}};

  const Config DEVNET = {
      "https://mango.devnet.rpcpool.com",
      "Ec2enZyoC4nGpEfu2sUNAa2nUGJHWxoUWYSEJ2hNTWTA",
      "4skJ85cdxQAFVKbcGgfun8iZPL7BadVYXG3kGEGkufqA",
      {6, 6, 6, 9, 6, 6, 6, 6, 6, 9, 8, 8, 8, 0, 0, 6},
      {"MNGO", "BTC", "ETH", "SOL", "SRM", "RAY", "USDT", "ADA", "FTT", "AVAX", "LUNA", "BNB", "MATIC", "", "", "USDC"}};

// all rust structs assume padding to 8
#pragma pack(push, 8)

  struct MetaData
  {
    uint8_t dataType;
    uint8_t version;
    uint8_t isInitialized;
    uint8_t padding[5];
  };

  struct TokenInfo
  {
    solana::PublicKey mint;
    solana::PublicKey rootBank;
    uint8_t decimals;
    uint8_t padding[7];
  };

  struct SpotMarketInfo
  {
    solana::PublicKey spotMarket;
    i80f48 maintAssetWeight;
    i80f48 initAssetWeight;
    i80f48 maintLiabWeight;
    i80f48 initLiabWeight;
    i80f48 liquidationFee;
  };

  struct PerpMarketInfo
  {
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

  struct MangoGroup
  {
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

  struct LiquidityMiningInfo
  {
    i80f48 rate;
    i80f48 maxDepthBps;
    uint64_t periodStart;
    uint64_t targetPeriodLength;
    uint64_t mngoLeft;
    uint64_t mngoPerPeriod;
  };

  struct PerpMarket
  {
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

  struct EventQueueHeader
  {
    MetaData metaData;
    uint64_t head;
    uint64_t count;
    uint64_t seqNum;
  };

  enum EventType : uint8_t
  {
    Fill,
    Out,
    Liquidate
  };

  enum Side : uint8_t
  {
    Buy,
    Sell
  };

  struct AnyEvent
  {
    EventType eventType;
    uint8_t padding[EVENT_SIZE - 1];
  };

  struct FillEvent
  {
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

  struct LiquidateEvent
  {
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

  struct OutEvent
  {
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

  struct EventQueue
  {
    EventQueueHeader header;
    AnyEvent items[EVENT_QUEUE_SIZE];
  };

#pragma pack(pop)

// instructions are even tighter packed, every byte counts
#pragma pack(push, 1)

  namespace ix
  {
    template <typename T>
    std::vector<uint8_t> toBytes(const T &ref)
    {
      const auto bytePtr = (uint8_t *)&ref;
      return std::vector<uint8_t>(bytePtr, bytePtr + sizeof(T));
    }

    std::pair<int64_t, int64_t> uiToNativePriceQuantity(
        double price,
        double quantity,
        const Config &config,
        const int marketIndex,
        const PerpMarket &market)
    {
      const int64_t baseUnit = pow(10LL, config.decimals[marketIndex]);
      const int64_t quoteUnit = pow(10LL, config.decimals[QUOTE_INDEX]);
      const auto nativePrice = ((int64_t)(price * quoteUnit)) * market.baseLotSize / (market.quoteLotSize * baseUnit);
      const auto nativeQuantity = ((int64_t)(quantity * baseUnit)) / market.baseLotSize;
      return std::pair<int64_t, int64_t>(nativePrice, nativeQuantity);
    };

    enum OrderType : uint8_t
    {
      Limit = 0,
      IOC = 1,
      PostOnly = 2,
      Market = 3,
      PostOnlySlide = 4
    };

    struct PlacePerpOrder
    {
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
        const PlacePerpOrder &ixData,
        const solana::PublicKey &ownerPk,
        const solana::PublicKey &accountPk,
        const solana::PublicKey &marketPk,
        const PerpMarket &market,
        const solana::PublicKey &groupPk,
        const MangoGroup &group,
        const solana::PublicKey &programPk)
    {
      std::vector<solana::AccountMeta> accs = {
          {groupPk, false, false},
          {accountPk, false, true},
          {ownerPk, true, false},
          {group.mangoCache, false, false},
          {marketPk, false, true},
          {market.bids, false, true},
          {market.asks, false, true},
          {market.eventQueue, false, true},
      };
      for (int i = 0; i < MAX_PAIRS; ++i)
      {
        accs.push_back({{}, false, false});
      }
      return {programPk, accs, toBytes(ixData)};
    };

    struct CancelAllPerpOrders
    {
      static const uint32_t CODE = 39;
      uint32_t ixs = CODE;
      // limit the number of cancelled orders to stay within compute limits
      uint8_t limit;
    };

    solana::Instruction cancelAllPerpOrdersInstruction(
        const CancelAllPerpOrders &ixData,
        const solana::PublicKey &ownerPk,
        const solana::PublicKey &accountPk,
        const solana::PublicKey &marketPk,
        const PerpMarket &market,
        const solana::PublicKey &groupPk,
        const solana::PublicKey &programPk)
    {
      const std::vector<solana::AccountMeta> accs = {
          {groupPk, false, false},
          {accountPk, false, true},
          {ownerPk, true, false},
          {marketPk, false, true},
          {market.bids, false, true},
          {market.asks, false, true},
      };
      return {programPk, accs, toBytes(ixData)};
    };
  }

#pragma pack(pop)

}
