#include <sodium.h>
#include <cpr/cpr.h>
#include "Connection.hpp"
#include "Account.hpp"
#include "Types.h"
#include "Base64.hpp"

namespace sol {
    Connection::Connection(std::string rpc_url, std::string commitment):
    rpc_url_(std::move(rpc_url)),
    commitment_(std::move(commitment))
    {
        auto sodium_result = sodium_init();
        if (sodium_result < -1)
            throw std::runtime_error("Error initializing sodium: " + std::to_string(sodium_result));

    }

    std::optional<AccountInfo> Connection::getAccountInfo(const PublicKey& account){
        const json req = Account::getAccountInfoRequest(account, commitment_);
        cpr::AsyncResponse fr = cpr::PostAsync(cpr::Url{rpc_url_},
                                    cpr::Body{req.dump()},
                                    cpr::Header{{"Content-Type", "application/json"}});
        cpr::Response r = fr.get();
        assert(r.status_code == 200);
        if (r.text.empty()){
            // Account doesn't exist
            return std::nullopt;
        }
        json res = json::parse(r.text);
        const std::string encoded = res["result"]["value"]["data"][0];
        const std::string decoded = Base64::b64decode(encoded);
        std::string result;
        memcpy(&result, decoded.data(), sizeof(std::string));
        const bool executable = res["result"]["value"]["executable"];
        const uint64_t lamports = res["result"]["value"]["lamports"];
        const PublicKey owner = PublicKey::fromBase58(res["result"]["value"]["owner"]);
        const uint64_t rentEpoch = res["result"]["value"]["rentEpoch"];

        return AccountInfo{
            executable,
            owner,
            lamports,
            result,
            rentEpoch
        };
    }
    BlockHash Connection::getRecentBlockhash(){
        // TODO: Implement this
        return "";
    }
    std::string Connection::signAndSendTransaction(const KeyPair& keypair, Instruction& instruction,
                                       bool skipPreflight,
                                       const std::string &preflightCommitment){
        // TODO: Implement this
        return "";
    }
}
