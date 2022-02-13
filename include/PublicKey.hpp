#pragma once

#include <iostream>
#include <sodium.h>
#include <string>
#include "Base58.hpp"
#include "Base64.hpp"

namespace sol {
    struct PublicKey {
        static const size_t SIZE = crypto_sign_PUBLICKEYBYTES;

        uint8_t data[SIZE];

        static PublicKey empty() {
            PublicKey result = {};
            memset(result.data, 0, SIZE);
            return result;
        }

        static PublicKey fromBase58(const std::string& b58) {
            PublicKey result = {};
            const std::string decoded = Base58::b58decode(b58);
            memcpy(result.data, decoded.data(), SIZE);
            return result;
        }

        bool operator==(const PublicKey& other) const {
            return 0 == memcmp(data, other.data, SIZE);
        }

        std::string toBase58() const {
            return Base58::b58encode(std::string(data, data + SIZE));
        }
    };

}