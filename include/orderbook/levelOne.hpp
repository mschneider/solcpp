#pragma once

namespace mango_v3 {
namespace book {

struct levelOne {
  uint64_t highestBid;
  uint64_t highestBidSize;
  uint64_t lowestAsk;
  uint64_t lowestAskSize;
  double midPoint;
  double spreadBps;

  bool valid() const {
    return ((highestBid && lowestAsk) && (lowestAsk > highestBid)) ? true
                                                                   : false;
  }
};

}  // namespace book
}  // namespace mango_v3
