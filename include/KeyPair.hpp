#pragma once

#include <fstream>
#include "PublicKey.hpp"
#include "PrivateKey.hpp"
#include "Types.h"

namespace sol {
    struct KeyPair
    {
        PublicKey publicKey;
        PrivateKey privateKey;

        static KeyPair fromFile(const std::string &path)
        {
            KeyPair result = {};
            std::ifstream fileStream(path);
            std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
            result.privateKey = PrivateKey::fromJson(json::parse(fileContent));
            crypto_sign_ed25519_sk_to_pk(result.publicKey.data, result.privateKey.data);
            return result;
        }
    };
}