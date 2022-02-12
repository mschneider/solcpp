#pragma once

#include <sodium.h>
#include <string>
#include "base58.hpp"
#include "base64.hpp"

namespace solana {
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
            const std::string decoded = b58decode(b58);
            memcpy(result.data, decoded.data(), SIZE);
            return result;
        }

        bool operator==(const PublicKey& other) const {
            return 0 == memcmp(data, other.data, SIZE);
        }
        std::ostream& operator<<(ostream& os, const PublicKey& other){
            os << other.toBase58();
            return os;
        }

        std::string toBase58() const {
            return b58encode(std::string(data, data + SIZE));
        }
    };
}