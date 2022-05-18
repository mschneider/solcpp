#pragma once

#include "solana.hpp"

namespace serum_v3 {
enum class AccountFlags : uint64_t {
  Initialized = 1 << 0,
  Market = 1 << 1,
  OpenOrders = 1 << 2,
  RequestQueue = 1 << 3,
  EventQueue = 1 << 4,
  Bids = 1 << 5,
  Asks = 1 << 6,
  Disabled = 1 << 7,
  Closed = 1 << 8,
  Permissioned = 1 << 9,
  CrankAuthorityRequired = 1 << 10
};
constexpr AccountFlags operator&(AccountFlags a, AccountFlags b) {
  return (AccountFlags)((uint64_t)a & (uint64_t)b);
}
#pragma pack(push, 1)
struct OpenOrders {
  uint8_t padding0[5];
  AccountFlags accountFlags;
  solana::PublicKey market;
  solana::PublicKey owner;
  uint64_t baseTokenFree;
  uint64_t baseTokenTotal;
  uint64_t quoteTokenFree;
  uint64_t quoteTokenTotal;
  __uint128_t freeSlotBits;
  __uint128_t isBidBits;
  __uint128_t orders[128];
  uint64_t clientIds[128];
  uint64_t referrerRebatesAccrued;
  uint8_t padding1[7];
};
#pragma pack(pop)
}  // namespace serum_v3