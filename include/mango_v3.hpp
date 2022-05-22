#pragma once

#include <cstdint>
#include <memory>
#include <stack>
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
const int BOOK_NODE_SIZE = 88;
const int BOOK_SIZE = 1024;
const int MAXIMUM_NUMBER_OF_BLOCKS_FOR_TRANSACTION = 152;

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

enum class HealthType { Unknown, Init, Maint };

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

// todo: change to scoped enum class
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

// todo: change to scoped enum class
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

// todo: change to scoped enum class
enum NodeType : uint32_t {
  Uninitialized = 0,
  InnerNode,
  LeafNode,
  FreeNode,
  LastFreeNode
};

struct AnyNode {
  NodeType tag;
  uint8_t padding[BOOK_NODE_SIZE - 4];
};

struct InnerNode {
  NodeType tag;
  uint32_t prefixLen;
  __uint128_t key;
  uint32_t children[2];
  uint8_t padding[BOOK_NODE_SIZE - 32];
};

struct LeafNode {
  NodeType tag;
  uint8_t ownerSlot;
  uint8_t orderType;
  uint8_t version;
  uint8_t timeInForce;
  __uint128_t key;
  solana::PublicKey owner;
  uint64_t quantity;
  uint64_t clientOrderId;
  uint64_t bestInitial;
  uint64_t timestamp;
};

struct FreeNode {
  NodeType tag;
  uint32_t next;
  uint8_t padding[BOOK_NODE_SIZE - 8];
};

struct L1Orderbook {
  uint64_t highestBid = 0;
  uint64_t highestBidSize = 0;
  uint64_t lowestAsk = 0;
  uint64_t lowestAskSize = 0;
  double midPoint = 0.0;
  double spreadBps = 0.0;

  bool valid() const {
    return ((highestBid && lowestAsk) && (lowestAsk > highestBid)) ? true
                                                                   : false;
  }
};

class BookSide {
 public:
  using order_t = struct LeafNode;
  using orders_t = std::vector<order_t>;

  BookSide(Side side, uint8_t maxBookDelay = 255)
      : side(side), maxBookDelay(maxBookDelay) {}

  auto getMaxTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto nowUnix =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();

    auto maxTimestamp = nowUnix - maxBookDelay;
    auto iter = BookSide::BookSideRaw::iterator(side, *raw);
    while (iter.stack.size() > 0) {
      if ((*iter).tag == NodeType::LeafNode) {
        const auto leafNode =
            reinterpret_cast<const struct LeafNode*>(&(*iter));
        if (leafNode->timestamp > maxTimestamp) {
          maxTimestamp = leafNode->timestamp;
        }
      }
      ++iter;
    }
    return maxTimestamp;
  }

  bool update(const std::string& decoded) {
    if (decoded.size() != sizeof(BookSideRaw)) {
      throw std::runtime_error("invalid response length " +
                               std::to_string(decoded.size()) + " expected " +
                               std::to_string(sizeof(BookSideRaw)));
    }
    memcpy(&(*raw), decoded.data(), sizeof(BookSideRaw));

    auto iter = BookSide::BookSideRaw::iterator(side, *raw);
    orders_t newOrders;

    const auto now = getMaxTimestamp();

    while (iter.stack.size() > 0) {
      if ((*iter).tag == NodeType::LeafNode) {
        const auto leafNode =
            reinterpret_cast<const struct LeafNode*>(&(*iter));
        const auto isValid =
            !leafNode->timeInForce ||
            ((leafNode->timestamp + leafNode->timeInForce) > now);
        if (isValid) {
          newOrders.push_back(*leafNode);
        }
      }
      ++iter;
    }

    if (!newOrders.empty()) {
      orders = std::make_shared<orders_t>(std::move(newOrders));
      return true;
    } else {
      return false;
    }
  }

  std::shared_ptr<order_t> getBestOrder() const {
    return orders->empty() ? nullptr
                           : std::make_shared<order_t>(orders->front());
  }

  uint64_t getVolume(uint64_t price) const {
    if (side == Side::Buy) {
      return getVolume<std::greater_equal<uint64_t>>(price);
    } else {
      return getVolume<std::less_equal<uint64_t>>(price);
    }
  }

 private:
  template <typename Op>
  uint64_t getVolume(uint64_t price) const {
    Op operation;
    uint64_t volume = 0;
    for (auto&& order : *orders) {
      auto orderPrice = (uint64_t)(order.key >> 64);
      if (operation(orderPrice, price)) {
        volume += order.quantity;
      } else {
        break;
      }
    }
    return volume;
  }

  struct BookSideRaw {
    MetaData metaData;
    uint64_t bumpIndex;
    uint64_t freeListLen;
    uint32_t freeListHead;
    uint32_t rootNode;
    uint64_t leafCount;
    AnyNode nodes[BOOK_SIZE];

    struct iterator {
      Side side;
      const BookSideRaw& bookSide;
      std::stack<uint32_t> stack;
      uint32_t left, right;

      iterator(Side side, const BookSideRaw& bookSide)
          : side(side), bookSide(bookSide) {
        stack.push(bookSide.rootNode);
        left = side == Side::Buy ? 1 : 0;
        right = side == Side::Buy ? 0 : 1;
      }

      bool operator==(const iterator& other) const {
        return &bookSide == &other.bookSide && stack.top() == other.stack.top();
      }

      iterator& operator++() {
        if (stack.size() > 0) {
          const auto& elem = **this;
          stack.pop();

          if (elem.tag == NodeType::InnerNode) {
            const auto innerNode =
                reinterpret_cast<const struct InnerNode*>(&elem);
            stack.push(innerNode->children[right]);
            stack.push(innerNode->children[left]);
          }
        }
        return *this;
      }

      const AnyNode& operator*() const { return bookSide.nodes[stack.top()]; }
    };
  };

  const Side side;
  uint8_t maxBookDelay;
  std::shared_ptr<BookSideRaw> raw = std::make_shared<BookSideRaw>();
  std::shared_ptr<orders_t> orders = std::make_shared<orders_t>();
};

class Trades {
 public:
  auto getLastTrade() const { return lastTrade; }

  bool update(const std::string& decoded) {
    const auto events = reinterpret_cast<const EventQueue*>(decoded.data());
    const auto seqNumDiff = events->header.seqNum - lastSeqNum;
    const auto lastSlot =
        (events->header.head + events->header.count) % EVENT_QUEUE_SIZE;

    bool gotLatest = false;
    if (events->header.seqNum > lastSeqNum) {
      for (int offset = seqNumDiff; offset > 0; --offset) {
        const auto slot =
            (lastSlot - offset + EVENT_QUEUE_SIZE) % EVENT_QUEUE_SIZE;
        const auto& event = events->items[slot];

        if (event.eventType == EventType::Fill) {
          const auto& fill = (FillEvent&)event;
          lastTrade = std::make_shared<FillEvent>(fill);
          gotLatest = true;
        }
        // no break; let's iterate to the last fill to get the latest fill
        // order
      }
    }

    lastSeqNum = events->header.seqNum;
    return gotLatest;
  }

 private:
  uint64_t lastSeqNum = INT_MAX;
  std::shared_ptr<FillEvent> lastTrade;
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
