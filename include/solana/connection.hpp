#pragma once

#include <sodium.h>
#include <string>
#include <memory>
#include "publickey.hpp"
#include "keypair.hpp"
#include "transaction.hpp"
#include "types.hpp"

namespace sol {

    constexpr auto LAMPORTS_PER_SOL = 1000000000;
    const std::string mainnet_beta_url = "https://api.mainnet-beta.solana.com:8899";

    class Connection: std::enable_shared_from_this<Connection> {
    public:
        /// Create a new Connection instance and initialize sodium
        /// \param rpc_url RPC url to connect to
        /// \param commitment The commitment level to use
        Connection(std::string rpc_url = mainnet_beta_url, std::string commitment = "finalized");

        // Solana RPC methods - names should match Solana specification for discoverability

        /// Fetch account info for the publicKey, base-58 encoded
        /// \param publicKey
        /// \return The requested info or std::nullopt if the account doesn't exist
        std::optional<AccountInfo> getAccountInfo(const PublicKey& account);
        /// Return the recent blockhash. Deprecated.
        /// \return The recent blockhash
        [[deprecated("Will be removed in v2.0. Use getLatestBlockhash(v1.9+)")]]
        [[maybe_unused]] BlockHash getRecentBlockhash();
        /// Signs and sends a transaction for processing.
        /// \param keypair The signer payer
        /// \param instruction Encoded instruction data
        /// \param skipPreflight if true, skip the preflight transaction checks (default: false)
        /// \param preflightCommitment  Commitment level to use for preflight
        /// \return First Transaction Signature embedded in the transaction, as base-58 encoded string
        [[maybe_unused]] std::string signAndSendTransaction(const KeyPair& keypair, Instruction& instruction,
                                           bool skipPreflight = false,
                                           const std::string &preflightCommitment = "finalized");
        // TODO: Support batch transactions
        // TODO: Add more RPC methods here

    private:
        std::string rpc_url_;
        std::string commitment_;
    };
}
