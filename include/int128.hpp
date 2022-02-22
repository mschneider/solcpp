#pragma once

#include <iostream>
#include <sstream>
#include <string>

std::ostream &operator<<(std::ostream &out, __uint128_t x) {
  if (x >= 10) out << x / 10;
  return out << static_cast<unsigned>(x % 10);
}

std::ostream &operator<<(std::ostream &out, __int128_t x) {
  if (x < 0) {
    out << '-';
    x = -x;
  }
  return out << static_cast<__uint128_t>(x);
}

std::string to_string(__int128_t num) {
  std::stringstream ss;
  ss << num;
  return ss.str();
}
