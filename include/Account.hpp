#pragma once

#include "PublicKey.hpp"
#include "Request.hpp"

namespace solana {
    struct AccountMeta
    {
        PublicKey pubkey;
        bool isSigner;
        bool isWritable;

        bool operator<(const AccountMeta &other) const
        {
            return (isSigner > other.isSigner) || (isWritable > other.isWritable);
        }
    };
    struct AccountInfo {
        bool executable;
        PublicKey owner;
        uint64_t lamports;
        std::string data;
        uint64_t rentEpoch;
    };
    class Account{
        static json getAccountInfoRequest(const PublicKey &account, const std::string &encoding = "base64")
        {
            const json params = {
                    account.toBase58(),
                    {{"encoding", encoding}}};

            return Request::fromJson("getAccountInfo", params);
        }
    };

}