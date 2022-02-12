#include <sodium.h>
#include <fmt/core.h>
#include <cpr/cpr.h>
#include "Connection.hpp"
#include "Account.hpp"

namespace solana {
    Connection::Connection(std::string& rpc_url, Commitment commitment):
    rpc_url_{rpc_url},
    commitment_{commitement}
    {
        auto sodium_result = sodium_init();
        if (sodium_result < -1)
            throw std::runtime_error(fmt::format("Error initializing sodium: {}", sodium_result));
    };
    Connection Connection::withRPC(std::string& rpc_url){
        return Connection{rpc_url, commitment};
    }
    Connection Connection::withCommitment(Commitment commitment){
        return Connection{rpc_url_, commitment};
    }
    std::optional<AccountInfo> Connection::getAccountInfo(const PublicKey& account){
        const json req = Account::getAccountInfoRequest(account);
        cpr::AsyncResponse fr = cpr::PostAsync(cpr::Url{endpoint},
                                    cpr::Body{req.dump()},
                                    cpr::Header{{"Content-Type", "application/json"}});
        cpr::Response r = fr.get();
        assert(r.status_code == 200);
        if (r.text.empty()){
            return std::nullopt;
        }
        json res = json::parse(r.text);
        const std::string encoded = res["result"]["value"]["data"][0];
        const std::string decoded = b64decode(encoded);
        const bool executable = res["result"]["value"]["executable"];
        const uint64_t lamports = res["result"]["value"]["lamports"];
        const PublicKey owner = PublicKey::fromBase58(res["result"]["value"]["owner"]);
        const uint64_t rentEpoch = res["result"]["value"]["rentEpoch"];
        return AccountInfo{
            executable,
            owner,
            lamports,
            decoded,
            rentEpoch
        };
    }
    BlockHash Connection::getRecentBlockhash(){

    }
    std::string Connection::signAndSendTransaction(const KeyPair& keypair, Instruction& instruction,
                                       bool skipPreflight = false,
                                       const Commitment &preflightCommitment = Commitment::Finalized){

    }
}
