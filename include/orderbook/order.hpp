#pragma once

namespace mango_v3 {
namespace orderbook {

struct order {
  order(uint64_t price, uint64_t quantity) : price(price), quantity(quantity) {}

  bool operator<(const order& compare) const {
    return (price < compare.price) ? true : false;
  }

  bool operator>(const order& compare) const {
    return (price > compare.price) ? true : false;
  }

  uint64_t price;
  uint64_t quantity;
};

}  // namespace orderbook
}  // namespace mango_v3
