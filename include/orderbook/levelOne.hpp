#pragma once

namespace mango_v3 {
namespace book {

struct levelOne {
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

}  // namespace book
}  // namespace mango_v3
