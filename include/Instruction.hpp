#pragma once

#include <vector>
#include <algorithm>
#include <fmt/core.h>
#include "PublicKey.hpp"
#include "Account.hpp"
#include "PushCompact.hpp"
namespace solana {
    struct Instruction
    {
        PublicKey programId;
        std::vector<AccountMeta> accounts;
        std::vector<uint8_t> data;
    };
    struct CompiledInstruction
    {
        uint8_t programIdIndex;
        std::vector<uint8_t> accountIndices;
        std::vector<uint8_t> data;

        static CompiledInstruction fromInstruction(const Instruction& ix, const std::vector<PublicKey>& accounts)
        {
            const auto programIdIt = std::find(accounts.begin(), accounts.end(), ix.programId);
            if (programIdIt == accounts.end()){
                throw std::invalid_argument(fmt::format("ProgramId not in accounts: {}", ix.programId));
            }
            const auto programIdIndex = static_cast<uint8_t>(programIdIt - accounts.begin());
            std::vector<uint8_t> accountIndices;
            for (const auto &account : ix.accounts)
            {
                const auto accountPkIt = std::find(accounts.begin(), accounts.end(), account.pubkey);
                if (accountPkIt == accounts.end()){
                    throw std::invalid_argument(fmt::format("ProgramId not in accounts: {}", account.pubkey));
                }
                accountIndices.push_back(accountPkIt - accounts.begin());
            }
            return {programIdIndex, accountIndices, ix.data};
        }

        void serializeTo(std::vector<uint8_t>& buffer) const
        {
            buffer.push_back(programIdIndex);
            pushCompactVecU8(accountIndices, buffer);
            pushCompactVecU8(data, buffer);
        }
    };
}