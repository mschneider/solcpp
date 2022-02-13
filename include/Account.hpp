#pragma once

#include "PublicKey.hpp"
#include "Request.hpp"
#include "Types.h"

namespace sol {
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
    // TODO: Template AccountInfo::data
    struct AccountInfo {
        [[maybe_unused]] bool executable;
        PublicKey owner;
        uint64_t lamports;
        std::string data;
        [[maybe_unused]] uint64_t rentEpoch;
    };
    class Account {
    public:
        static json getAccountInfoRequest(const PublicKey &account,
                                          const std::string commitment,
                                          const std::string &encoding = "base64")
        {
            const json params = {
                    account.toBase58(),
                    {
                        {"encoding", encoding},
                        {"commitment", commitment},
                    },

            };
            return Request::fromJson("getAccountInfo", params);
        }
    };

}