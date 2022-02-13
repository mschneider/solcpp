#pragma once

#include <string>
#include <vector>
#include <sodium.h>
#include <iostream>
#include "Types.h"

namespace sol {
    struct PrivateKey {
        static const size_t SIZE = crypto_sign_SECRETKEYBYTES;

        uint8_t data[SIZE];

        static PrivateKey fromJson(const json bytes) {
            PrivateKey result = {};
            const std::vector <uint8_t> asVec = bytes;
            memcpy(result.data, asVec.data(), SIZE);
            return result;
        }

        std::vector <uint8_t> signMessage(const std::vector <uint8_t> message) const {
            uint8_t sig[crypto_sign_BYTES];
            unsigned long long sigSize;
            if (0 != crypto_sign_detached(sig, &sigSize, message.data(), message.size(), data)) {
                std::cerr << "could not sign tx with private key" << std::endl;
                return {};
            }

            return std::vector<uint8_t>(sig, sig + sigSize);
        }
    };
}