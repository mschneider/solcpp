#pragma once

#include <sodium.h>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "PublicKey.hpp"
#include "KeyPair.hpp"
#include "Transaction.hpp"


namespace solana{
    constexpr auto LAMPORTS_PER_SOL = 1000000000;
    constexpr std:string mainnet_beta_url = "https://api.mainnet-beta.solana.com:8899";
    enum class Commitment {
        Finalized,
        Confirmed,
        Processed
    };
    using json = nlohmann::json;
    class Connection: std::enable_shared_from_this{
    public:
        /// Create a new Connection isntance and initialize sodium
        /// \param rpc_url RPC url to connect to
        /// \param commitment The commitment level to use
        Connection(std::string rpc_url = mainnet_beta_url, Commitment commitment = Commitment::Finalized);
        ///  Update the current RPC Url ony
        /// \param rpc_url The new RPC url
        /// \return Connection instance
        Connection withRPC(std::string& rpc_url) const;
        /// Update the current commitment only
        /// \param commitment The new commitment level
        /// \return Connection instance
        Connection withCommitment(Commitment commitment) const;

        ///
        /// Solana RPC methods - names should match Solana specification for discoverability
        ///

        /// Fetch account info for the publicKey, base-58 encoded
        /// \param publicKey
        /// \return The requested info or null if the account doesn't exist
        std::optional<AccountInfo> getAccountInfo(const PublicKey& publicKey);
        /// Return the recent blockhash. Deprecated.
        /// \return The recent blockhash
        [[deprecated("Will be removed in v2.0. Use getLatestBlockhash(v1.9+)")]]
        BlockHash getRecentBlockhash();
        /// Signs and sends a transaction for processing.
        /// \param keypair The signer payer
        /// \param instruction Encoded instruction data
        /// \param skipPreflight if true, skip the preflight transaction checks (default: false)
        /// \param preflightCommitment  Commitment level to use for preflight
        /// \return First Transaction Signature embedded in the transaction, as base-58 encoded string
        std::string signAndSendTransaction(const KeyPair& keypair, Instruction& instruction,
                                           bool skipPreflight = false,
                                           const Commitment &preflightCommitment = commitment_);
        //TODO: Add more RPC methods here


    private:
        std::string& rpc_url_;
        Commitment commitment_;
    };
}
