#include <solana.hpp>
namespace solana {
    class Solana {
    public:
        Solana(const std::string& rpc_url = MAINNET_BETA, const std::string& commitment = "finalized");

    private:
        std::string& rpc_url_;
        std::string& commitment_;
    };

    Solana::Solana(const std::string& rpc_url, const std::string& commitment):
    rpc_url_(std::move(rpc_url)), commitment_(std::move(commitment)){
        auto sodium_result = sodium_init();
        if (sodium_result < -1)
            throw std::runtime_error("Error initializing sodium: " + std::to_string(sodium_result));
    }
    json Solana::jsonRequest(const std::string &method, const json &params = nullptr)
    {
        json req = {
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", method}};
        if (params != nullptr)
            req["params"] = params;
        return req;
    }
    json Solana::accountSubscribeRequest(const std::string &account, const std::string &commitment = "processed", const std::string &encoding = "base64")
    {
        const json params = {
                account,
                {{"commitment", commitment}, {"encoding", encoding}}};

        return jsonRequest("accountSubscribe", params);
    }

    json Solana::getAccountInfoRequest(const std::string &account, const std::string &encoding = "base64")
    {
        const json params = {
                account,
                {{"encoding", encoding}}};

        return jsonRequest("getAccountInfo", params);
    }

    template <typename T>
    T Solana::getAccountInfo(const std::string &endpoint, const std::string &account)
    {
        const json req = getAccountInfoRequest(account);
        cpr::Response r = cpr::Post(cpr::Url{endpoint},
                                    cpr::Body{req.dump()},
                                    cpr::Header{{"Content-Type", "application/json"}});
        if (r.status_code != 200)
            throw std::runtime_error("unexpected status_code " + std::to_string(r.status_code));

        json res = json::parse(r.text);
        const std::string encoded = res["result"]["value"]["data"][0];
        const std::string decoded = b64decode(encoded);
        if (decoded.size() != sizeof(T))
            throw std::runtime_error("invalid response length " + std::to_string(decoded.size()) + " expected " + std::to_string(sizeof(T)));

        T result;
        memcpy(&result, decoded.data(), sizeof(T));
        return result;
    }

    json Solana::getRecentBlockhashRequest(const std::string &commitment = "finalized")
    {
        const json params = {
                {{"commitment", commitment}}};

        return jsonRequest("getRecentBlockhash", params);
    }

    PublicKey Solana::getRecentBlockhash(const std::string &endpoint, const std::string &commitment = "finalized")
    {
        const json req = getRecentBlockhashRequest(commitment);
        cpr::Response r = cpr::Post(cpr::Url{endpoint},
                                    cpr::Body{req.dump()},
                                    cpr::Header{{"Content-Type", "application/json"}});
        if (r.status_code != 200)
            throw std::runtime_error("unexpected status_code " + std::to_string(r.status_code));

        json res = json::parse(r.text);
        const std::string encoded = res["result"]["value"]["blockhash"];
        return PublicKey::fromBase58(encoded);
    }

    json Solana::getSignatureStatuses(const std::vector<std::string> &signatures, bool searchTransactionHistory = false)
    {
        const json params = {
                signatures,
                {{"searchTransactionHistory", searchTransactionHistory}}};

        return jsonRequest("getSignatureStatuses", params);
    }

    json Solana::sendTransactionRequest(const std::string &transaction, const std::string &encoding = "base58", bool skipPreflight = false, const std::string &preflightCommitment = "finalized")
    {
        const json params = {
                transaction,
                {{"encoding", encoding}, {"skipPreflight", skipPreflight}, {"preflightCommitment", preflightCommitment}}};

        return jsonRequest("sendTransaction", params);
    }

    std::string Solana::signAndSendTransaction(const std::string &endpoint, const Keypair &keypair, const CompiledTransaction &tx, bool skipPreflight = false, const std::string &preflightCommitment = "finalized")
    {
        std::vector<uint8_t> txBody;
        tx.serializeTo(txBody);

        const auto signature = keypair.privateKey.signMessage(txBody);
        const auto b58Sig = b58encode(std::string(signature.begin(), signature.end()));

        std::vector<uint8_t> signedTx;
        pushCompactU16(1, signedTx);
        signedTx.insert(signedTx.end(), signature.begin(), signature.end());
        signedTx.insert(signedTx.end(), txBody.begin(), txBody.end());

        const auto b64tx = b64encode(std::string(signedTx.begin(), signedTx.end()));
        const json req = solana::rpc::sendTransactionRequest(b64tx, "base64", skipPreflight, preflightCommitment);
        const std::string jsonSerialized = req.dump();

        cpr::Response r = cpr::Post(cpr::Url{endpoint},
                                    cpr::Body{jsonSerialized},
                                    cpr::Header{{"Content-Type", "application/json"}});
        if (r.status_code != 200)
            throw std::runtime_error("unexpected status_code " + std::to_string(r.status_code));

        json res = json::parse(r.text);
        if (b58Sig != res["result"])
            throw std::runtime_error("could not submit tx: " + r.text);

        return b58Sig;
    }
}