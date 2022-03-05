#pragma once

#include <cstdint>
#include <memory>
#include <stack>
#include <string>

#include "fixedp.h"
#include "int128.hpp"
#include "solana.hpp"

namespace mango_v3 {
using json = nlohmann::json;

const int MAX_TOKENS = 16;
const int MAX_PAIRS = 15;
const int QUOTE_INDEX = 15;
const int EVENT_SIZE = 200;
const int EVENT_QUEUE_SIZE = 256;
const int BOOK_NODE_SIZE = 88;
const int BOOK_SIZE = 1024;

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

// todo: change to scoped enum class
enum Side : uint8_t { Buy, Sell };

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

struct Order {
  Order(uint64_t price, uint64_t quantity) : price(price), quantity(quantity) {}
  uint64_t price = 0;
  uint64_t quantity = 0;
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
  BookSide(Side side) : side(side) {}

  bool handleMsg(const nlohmann::json &msg) {
    // ignore subscription confirmation
    const auto itResult = msg.find("result");
    if (itResult != msg.end()) {
      return false;
    }

    const std::string encoded = msg["params"]["result"]["value"]["data"][0];
    const std::string decoded = solana::b64decode(encoded);
    return update(decoded);
  }

  Order getBestOrder() const {
    return (!orders->empty()) ? orders->front() : Order(0, 0);
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
    for (auto &&order : *orders) {
      if (operation(order.price, price)) {
        volume += order.quantity;
      } else {
        break;
      }
    }
    return volume;
  }

  bool update(const std::string decoded) {
    if (decoded.size() != sizeof(BookSideRaw)) {
      throw std::runtime_error("invalid response length " +
                               std::to_string(decoded.size()) + " expected " +
                               std::to_string(sizeof(BookSideRaw)));
    }
    memcpy(&(*raw), decoded.data(), sizeof(BookSideRaw));

    auto iter = BookSide::BookSideRaw::iterator(side, *raw);
    std::vector<Order> newOrders;
    while (iter.stack.size() > 0) {
      if ((*iter).tag == NodeType::LeafNode) {
        const auto leafNode =
            reinterpret_cast<const struct LeafNode *>(&(*iter));
        const auto now = std::chrono::system_clock::now();
        const auto nowUnix =
            chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                .count();
        const auto isValid =
            !leafNode->timeInForce ||
            leafNode->timestamp + leafNode->timeInForce < nowUnix;
        if (isValid) {
          newOrders.emplace_back((uint64_t)(leafNode->key >> 64),
                                 leafNode->quantity);
        }
      }
      ++iter;
    }

    if (!newOrders.empty()) {
      orders = std::make_shared<std::vector<Order>>(std::move(newOrders));
      return true;
    } else {
      return false;
    }
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
      const BookSideRaw &bookSide;
      std::stack<uint32_t> stack;
      uint32_t left, right;

      iterator(Side side, const BookSideRaw &bookSide)
          : side(side), bookSide(bookSide) {
        stack.push(bookSide.rootNode);
        left = side == Side::Buy ? 1 : 0;
        right = side == Side::Buy ? 0 : 1;
      }

      bool operator==(const iterator &other) const {
        return &bookSide == &other.bookSide && stack.top() == other.stack.top();
      }

      iterator &operator++() {
        if (stack.size() > 0) {
          const auto &elem = **this;
          stack.pop();

          if (elem.tag == NodeType::InnerNode) {
            const auto innerNode =
                reinterpret_cast<const struct InnerNode *>(&elem);
            stack.push(innerNode->children[right]);
            stack.push(innerNode->children[left]);
          }
        }
        return *this;
      }

      const AnyNode &operator*() const { return bookSide.nodes[stack.top()]; }
    };
  };

  const Side side;
  std::shared_ptr<BookSideRaw> raw = std::make_shared<BookSideRaw>();
  std::shared_ptr<std::vector<Order>> orders =
      std::make_shared<std::vector<Order>>();
};

class Trades {
 public:
  auto getLastTrade() const { return latestTrade; }

  bool handleMsg(const nlohmann::json &msg) {
    // ignore subscription confirmation
    const auto itResult = msg.find("result");
    if (itResult != msg.end()) {
      return false;
    }

    // all other messages are event queue updates
    const std::string method = msg["method"];
    const int subscription = msg["params"]["subscription"];
    const int slot = msg["params"]["result"]["context"]["slot"];
    const std::string data = msg["params"]["result"]["value"]["data"][0];

    const auto decoded = solana::b64decode(data);
    const auto events = reinterpret_cast<const EventQueue *>(decoded.data());
    const auto seqNumDiff = events->header.seqNum - lastSeqNum;
    const auto lastSlot =
        (events->header.head + events->header.count) % EVENT_QUEUE_SIZE;

    bool gotLatest = false;
    if (events->header.seqNum > lastSeqNum) {
      for (int offset = seqNumDiff; offset > 0; --offset) {
        const auto slot =
            (lastSlot - offset + EVENT_QUEUE_SIZE) % EVENT_QUEUE_SIZE;
        const auto &event = events->items[slot];

        if (event.eventType == EventType::Fill) {
          const auto &fill = (FillEvent &)event;
          latestTrade = std::make_shared<uint64_t>(fill.price);
          gotLatest = true;
        }
        // no break; let's iterate to the last fill to get the latest fill order
      }
    }

    lastSeqNum = events->header.seqNum;
    return gotLatest;
  }

 private:
  uint64_t lastSeqNum = INT_MAX;
  std::shared_ptr<uint64_t> latestTrade = std::make_shared<uint64_t>(0);
};

#pragma pack(pop)

// instructions are even tighter packed, every byte counts
#pragma pack(push, 1)

namespace ix {
template <typename T>
std::vector<uint8_t> toBytes(const T &ref) {
  const auto bytePtr = (uint8_t *)&ref;
  return std::vector<uint8_t>(bytePtr, bytePtr + sizeof(T));
}

std::pair<int64_t, int64_t> uiToNativePriceQuantity(double price,
                                                    double quantity,
                                                    const Config &config,
                                                    const int marketIndex,
                                                    const PerpMarket &market) {
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
    const PlacePerpOrder &ixData, const solana::PublicKey &ownerPk,
    const solana::PublicKey &accountPk, const solana::PublicKey &marketPk,
    const PerpMarket &market, const solana::PublicKey &groupPk,
    const MangoGroup &group, const solana::PublicKey &programPk) {
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
    const CancelAllPerpOrders &ixData, const solana::PublicKey &ownerPk,
    const solana::PublicKey &accountPk, const solana::PublicKey &marketPk,
    const PerpMarket &market, const solana::PublicKey &groupPk,
    const solana::PublicKey &programPk) {
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
