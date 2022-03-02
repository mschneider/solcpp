#pragma once

#include <vector>

namespace mango_v3 {
namespace book {

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

struct order_container {
  std::vector<order> orders;

  order getBest() const {
    return (!orders.empty()) ? orders.front() : order{0, 0};
  }

  template <typename Op>
  uint64_t getVolume(uint64_t price) const {
    Op operation;
    uint64_t volume = 0;
    for (auto&& order : orders) {
      if (operation(order.price, price)) {
        volume += order.quantity;
      } else {
        break;
      }
    }
    return volume;
  }
};
}  // namespace book
}  // namespace mango_v3
