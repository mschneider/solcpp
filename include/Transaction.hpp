#pragma once

#include "PublicKey.hpp"
#include "Instruction.hpp"
#include "Account.hpp"
#include "PushCompact.hpp"
#include "Types.h"

namespace sol {
    struct CompiledTransaction
    {
        PublicKey recentBlockhash;
        std::vector<PublicKey> accounts;
        std::vector<CompiledInstruction> instructions;
        uint8_t requiredSignatures;
        uint8_t readOnlySignedAccounts;
        uint8_t readOnlyUnsignedAccounts;

        static CompiledTransaction fromInstructions(const std::vector<Instruction> &instructions, const PublicKey &payer, const PublicKey &blockhash)
        {
            // collect all program ids and accounts including the payer
            std::vector<AccountMeta> allMetas = {{payer, true, true}};
            for (const auto &instruction : instructions)
            {
                allMetas.insert(allMetas.end(), instruction.accounts.begin(), instruction.accounts.end());
                allMetas.push_back({instruction.programId, false, false});
            }

            // merge account metas referencing the same acc/pubkey, assign maximum privileges
            std::vector<AccountMeta> uniqueMetas;
            for (const auto &meta : allMetas)
            {
                auto dup = std::find_if(uniqueMetas.begin(), uniqueMetas.end(), [&meta](const auto &u)
                { return u.pubkey == meta.pubkey; });

                if (dup == uniqueMetas.end())
                {
                    uniqueMetas.push_back(meta);
                }
                else
                {
                    dup->isSigner |= meta.isSigner;
                    dup->isWritable |= meta.isWritable;
                }
            }

            // sort using operator< to establish order: signer+writable, signers, writables, others
            std::sort(uniqueMetas.begin(), uniqueMetas.end());

            uint8_t requiredSignatures = 0;
            uint8_t readOnlySignedAccounts = 0;
            uint8_t readOnlyUnsignedAccounts = 0;
            std::vector<PublicKey> accounts;
            for (const auto &meta : uniqueMetas)
            {
                accounts.push_back(meta.pubkey);
                if (meta.isSigner)
                {
                    requiredSignatures++;
                    if (!meta.isWritable)
                    {
                        readOnlySignedAccounts++;
                    }
                }
                else if (!meta.isWritable)
                {
                    readOnlyUnsignedAccounts++;
                }
            }

            // dictionary encode individual instructions
            std::vector<CompiledInstruction> cixs;
            for (const auto &instruction : instructions)
            {
                cixs.push_back(CompiledInstruction::fromInstruction(instruction, accounts));
            }
            return {blockhash, accounts, cixs, requiredSignatures, readOnlySignedAccounts, readOnlyUnsignedAccounts};
        };

        void serializeTo(std::vector<uint8_t> &buffer) const
        {
            buffer.push_back(requiredSignatures);
            buffer.push_back(readOnlySignedAccounts);
            buffer.push_back(readOnlyUnsignedAccounts);

            PushCompact::pushCompactU16(accounts.size(), buffer);
            for (const auto &account : accounts)
            {
                buffer.insert(buffer.end(), account.data, account.data + PublicKey::SIZE);
            }

            buffer.insert(buffer.end(), recentBlockhash.data, recentBlockhash.data + PublicKey::SIZE);

            PushCompact::pushCompactU16(instructions.size(), buffer);
            for (const auto &instruction : instructions)
            {
                instruction.serializeTo(buffer);
            }
        };
    };
}