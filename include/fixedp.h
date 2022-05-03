/*
 * "fixed.h" by David Flemström is licensed under a
 * Creative Commons Attribution-Share Alike 3.0 Unported License.
 *
 * Based on a work at http://www.codef00.com/code/, which is:
 * Copyright (c) 2008
 * Evan Teran
 * See the above link for the full copyright text.
 */

#ifndef FIXEDP_H
#define FIXEDP_H

#include <stdint.h>

#include <cstddef>

using namespace std;

// There must exist a SizeInfo implementation for your wanted fixed point size.
// It should e.g. not be possible to do fixed<2, 3> since there is no 5-bit type
// to store the value in. (it is possible for the user to add additional
// SizeInfo implementations of course)
template <bool S, size_t A>
struct FixedPSizeInfo;

#define FIXEDP_SIZE(x) (sizeof(x) * 8)
#define FIXEDP_NEXT_SIZE(x) (sizeof(x) * 16)
#define FIXEDP_PREV_SIZE(x) (sizeof(x) * 4)

// Declares a SizeInfo with the given signedness S and base type T
#define FIXEDP_MK_SIZE_INFO(S, T)                                 \
  template <>                                                     \
  struct FixedPSizeInfo<S, FIXEDP_SIZE(T)> {                      \
    typedef T ValType;                                            \
    static const size_t VAL_SIZE = FIXEDP_SIZE(T);                \
    typedef FixedPSizeInfo<S, FIXEDP_NEXT_SIZE(T)> NEXT_SIZEINFO; \
    typedef FixedPSizeInfo<S, FIXEDP_PREV_SIZE(T)> PREV_SIZEINFO; \
  }

#define FIXEDP_MK_DOUBLE_SIZE_INFO(S, T)                          \
  template <>                                                     \
  struct FixedPSizeInfo<S, FIXEDP_SIZE(T) * 2> {                  \
    typedef T ValType;                                            \
    static const size_t VAL_SIZE = FIXEDP_SIZE(T) * 2;            \
    typedef FixedPSizeInfo<S, FIXEDP_NEXT_SIZE(T)> NEXT_SIZEINFO; \
    typedef FixedPSizeInfo<S, FIXEDP_PREV_SIZE(T)> PREV_SIZEINFO; \
  }

// fake sizeinfo
FIXEDP_MK_DOUBLE_SIZE_INFO(true, __int128_t);
FIXEDP_MK_DOUBLE_SIZE_INFO(false, __uint128_t);

// Implicit SizeInfos for default types (128, 64, 32, 16 and 8-bit)
FIXEDP_MK_SIZE_INFO(true, __int128_t);
FIXEDP_MK_SIZE_INFO(false, __uint128_t);
FIXEDP_MK_SIZE_INFO(true, int64_t);
FIXEDP_MK_SIZE_INFO(false, uint64_t);
FIXEDP_MK_SIZE_INFO(true, int32_t);
FIXEDP_MK_SIZE_INFO(false, uint32_t);
FIXEDP_MK_SIZE_INFO(true, int16_t);
FIXEDP_MK_SIZE_INFO(false, uint16_t);
FIXEDP_MK_SIZE_INFO(true, int8_t);
FIXEDP_MK_SIZE_INFO(false, uint8_t);

#undef FIXEDP_SIZE
#undef FIXEDP_NEXT_SIZE
#undef FIXEDP_PREV_SIZE
#undef FIXEDP_MK_SIZE_INFO

template <class B, class N>
struct FixedPConvert {
  static B convert(const N &rhs) { return static_cast<B>(rhs); }
};

template <bool S, size_t I, size_t F>
class fixedp {
 public:
  // Define some values that can be used from the outside for polymorphism:
  static const size_t INT_SIZE = I;
  static const size_t FRACT_SIZE = F;
  static const size_t TOTAL_SIZE = I + F;
  static const bool SIGNED = S;

  // Get a SizeInfo for our size
  typedef FixedPSizeInfo<SIGNED, TOTAL_SIZE> ValTypeInfo;

  // Get a type with the correct size to use for storing stuff
  typedef typename ValTypeInfo::ValType ValType;
  typedef typename ValTypeInfo::NEXT_SIZEINFO::ValType NextValType;

  // Size of the valType in bits
  static const size_t VAL_SIZE = ValTypeInfo::VAL_SIZE;

  // 1 in our fixed's format
  static const ValType one = ValType(1) << FRACT_SIZE;

  fixedp() {}  // DON'T initialize data here; that's the users job!
  fixedp(const fixedp &other) : data(other.data) {}
  fixedp(const ValType &rhs) : data(rhs) {}

  explicit fixedp(int n) : data(n << FRACT_SIZE) {}
  explicit fixedp(float n) : data(static_cast<ValType>(n * one)) {}
  explicit fixedp(double n) : data(static_cast<ValType>(n * one)) {}
  explicit fixedp(unsigned int n) : data(ValType(n) << FRACT_SIZE) {}

#define FIXEDP_MK_CMP_OP(op) \
  inline bool operator op(const fixedp &o) const { return data op o.data; }
  FIXEDP_MK_CMP_OP(==)
  FIXEDP_MK_CMP_OP(!=)
  FIXEDP_MK_CMP_OP(<)
  FIXEDP_MK_CMP_OP(>)
  FIXEDP_MK_CMP_OP(>=)
  FIXEDP_MK_CMP_OP(<=)
#undef FIXEDP_MK_CMP_OP

  inline bool operator!() const { return !data; }
  inline fixedp operator~() const { return fixedp(~data); }
  inline fixedp &operator++() {
    data += one;
    return *this;
  }
  inline fixedp &operator--() {
    data -= one;
    return *this;
  }

#define FIXEDP_MK_BIN_OP(op)                         \
  inline fixedp &operator op##=(const fixedp &n) {   \
    data op## = n.data;                              \
    return *this;                                    \
  }                                                  \
  inline fixedp operator op(const fixedp &n) const { \
    fixedp x(*this);                                 \
    x op## = n;                                      \
    return x;                                        \
  }
  FIXEDP_MK_BIN_OP(+)
  FIXEDP_MK_BIN_OP(-)
  FIXEDP_MK_BIN_OP(&)
  FIXEDP_MK_BIN_OP(|)
  FIXEDP_MK_BIN_OP(^)
#undef FIXEDP_MK_BIN_OP

  inline fixedp &operator*=(const fixedp &n) {
    NextValType t(static_cast<NextValType>(data) *
                  static_cast<NextValType>(n.data));
    t >>= FRACT_SIZE;
    data = FixedPConvert<ValType, NextValType>::convert(t);
    return *this;
  }
  inline fixedp operator*(const fixedp &n) {
      fixedp x(*this);
      x *= n;
      return x;
  // return (this->toDouble())*n.toDouble();
  }

  inline fixedp &operator/=(const fixedp &n) {
    NextValType t(data);
    t <<= FRACT_SIZE;
    t /= n.data;
    data = FixedPConvert<ValType, NextValType>::convert(t);
    return *this;
  }
  inline fixedp operator/(const fixedp &n) {
    fixedp x(*this);
    x /= n;
    return x;
  }

  inline fixedp &operator>>=(const fixedp &n) {
    data >>= n.toInt();
    return *this;
  }
  inline fixedp operator>>(const fixedp &n) {
    fixedp x(*this);
    x >>= n;
    return x;
  }

  inline fixedp &operator<<=(const fixedp &n) {
    data <<= n.toInt();
    return *this;
  }
  inline fixedp operator<<(const fixedp &n) {
    fixedp x(*this);
    x <<= n;
    return x;
  }

  int toInt() const { return data & (ValType(-1) << FRACT_SIZE); }
  float toFloat() const { return static_cast<float>(data) / one; }
  double toDouble() const { return static_cast<double>(data) / one; }
  ValType raw() const { return data; }

 private:
  ValType data;
};

typedef fixedp<true, 4, 4> sfix8;
typedef fixedp<false, 4, 4> ufix8;

typedef fixedp<true, 8, 8> sfix16;
typedef fixedp<false, 8, 8> ufix16;

typedef fixedp<true, 16, 16> sfix32;
typedef fixedp<false, 16, 16> ufix32;

typedef fixedp<true, 80, 48> i80f48;

#endif  // FIXEDP_H