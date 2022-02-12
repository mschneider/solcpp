#pragma once

#include <fstream>
#include "PublicKey.hpp"
#include "PrivateKey.hpp"

namespace solana {
    struct Keypair
    {
        PublicKey publicKey;
        PrivateKey privateKey;

        static Keypair fromFile(const std::string &path)
        {
            Keypair result = {};
            std::ifstream fileStream(path);
            std::string fileContent(std::istreambuf_iterator<char>(fileStream), {});
            result.privateKey = PrivateKey::fromJson(json::parse(fileContent));
            crypto_sign_ed25519_sk_to_pk(result.publicKey.data, result.privateKey.data);
            return result;
        }
        static Keypair generate(){
            // TODO: Implement generate
            return nullptr;
        }
    };
}