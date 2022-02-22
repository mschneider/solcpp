#pragma once

/*
 * Copyright 2012-2014 Luke Dashjr
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the standard MIT license.
 */

#include <cstdbool>
#include <cstddef>
#include <cstdint>
#include <string>

namespace solana {
// clang-format off
    static constexpr int8_t b58digits_map[] = {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, -1, -1, -1, -1, -1, -1,
            -1, 9, 10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1,
            22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1, -1,
            -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46,
            47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1,
    };
// clang-format on

typedef uint64_t b58_maxint_t;
typedef uint32_t b58_almostmaxint_t;
#define b58_almostmaxint_bits (sizeof(b58_almostmaxint_t) * 8)
static const b58_almostmaxint_t b58_almostmaxint_mask =
    ((((b58_maxint_t)1) << b58_almostmaxint_bits) - 1);

inline bool b58tobin(void *bin, size_t *binszp, const char *b58, size_t b58sz) {
  size_t binsz = *binszp;
  const unsigned char *b58u = (const unsigned char *)b58;
  unsigned char *binu = (unsigned char *)bin;
  size_t outisz =
      (binsz + sizeof(b58_almostmaxint_t) - 1) / sizeof(b58_almostmaxint_t);
  b58_almostmaxint_t outi[outisz];
  b58_maxint_t t;
  b58_almostmaxint_t c;
  size_t i, j;
  uint8_t bytesleft = binsz % sizeof(b58_almostmaxint_t);
  b58_almostmaxint_t zeromask =
      bytesleft ? (b58_almostmaxint_mask << (bytesleft * 8)) : 0;
  unsigned zerocount = 0;

  if (!b58sz) b58sz = strlen(b58);

  for (i = 0; i < outisz; ++i) {
    outi[i] = 0;
  }

  // Leading zeros, just count
  for (i = 0; i < b58sz && b58u[i] == '1'; ++i) ++zerocount;

  for (; i < b58sz; ++i) {
    if (b58u[i] & 0x80)
      // High-bit set on invalid digit
      return false;
    if (b58digits_map[b58u[i]] == -1)
      // Invalid base58 digit
      return false;
    c = (unsigned)b58digits_map[b58u[i]];
    for (j = outisz; j--;) {
      t = ((b58_maxint_t)outi[j]) * 58 + c;
      c = t >> b58_almostmaxint_bits;
      outi[j] = t & b58_almostmaxint_mask;
    }
    if (c)
      // Output number too big (carry to the next int32)
      return false;
    if (outi[0] & zeromask)
      // Output number too big (last int32 filled too far)
      return false;
  }

  // std::cout << "b58tobin outisz:" << outisz << " b58sz:" << b58sz << "
  // bytesleft:" << (int)bytesleft << " outisz:" << outisz << " zerocount:" <<
  // zerocount << std::endl;

  j = 0;
  if (bytesleft) {
    for (i = bytesleft; i > 0; --i) {
      *(binu++) = (outi[0] >> (8 * (i - 1))) & 0xff;
    }
    ++j;
  }

  for (; j < outisz; ++j) {
    for (i = sizeof(*outi); i > 0; --i) {
      *(binu++) = (outi[j] >> (8 * (i - 1))) & 0xff;
    }
  }

  // Count canonical base58 byte count
  binu = (unsigned char *)bin;
  for (i = 0; i < binsz; ++i) {
    if (binu[i]) break;
    --*binszp;
  }
  *binszp += zerocount;

  return true;
}

static constexpr char b58digits_ordered[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

inline bool b58enc(char *b58, size_t *b58sz, const void *data, size_t binsz) {
  const uint8_t *bin = (const uint8_t *)data;
  int carry;
  size_t i, j, high, zcount = 0;
  size_t size;

  while (zcount < binsz && !bin[zcount]) ++zcount;

  size = (binsz - zcount) * 138 / 100 + 1;
  uint8_t buf[size];
  memset(buf, 0, size);

  for (i = zcount, high = size - 1; i < binsz; ++i, high = j) {
    for (carry = bin[i], j = size - 1; (j > high) || carry; --j) {
      carry += 256 * buf[j];
      buf[j] = carry % 58;
      carry /= 58;
      if (!j) {
        // Otherwise j wraps to maxint which is > high
        break;
      }
    }
  }

  for (j = 0; j < size && !buf[j]; ++j)
    ;

  if (*b58sz <= zcount + size - j) {
    *b58sz = zcount + size - j + 1;
    return false;
  }

  if (zcount) memset(b58, '1', zcount);
  for (i = zcount; j < size; ++i, ++j) b58[i] = b58digits_ordered[buf[j]];
  b58[i] = '\0';
  *b58sz = i + 1;

  return true;
}

// returns an empty string if b58 fails to decode
inline std::string b58decode(const std::string &b58) {
  size_t decodedSize = b58.size() * 733 / 1000 + 1;
  char decoded[decodedSize];
  const bool ok = b58tobin(decoded, &decodedSize, b58.c_str(), 0);
  if (ok) {
    // remove zero bytes in front of result
    size_t offset = (b58.size() * 733 / 1000 + 1) - decodedSize;
    return std::string(decoded + offset, decodedSize);
  } else
    return std::string();
}

// returns an empty string if str fails to encode
template <typename T>
inline std::string b58encode(const T &str) {
  size_t b58Size = str.size() * 138 / 100 + 1;
  std::string b58(b58Size, '\0');
  const bool ok = b58enc(b58.data(), &b58Size, str.data(), str.size());
  if (ok) {
    // remove zero byte at end of result
    return std::string(b58.data(), b58Size - 1);
  } else
    return std::string();
}
}  // namespace solana