#include <iostream>
#include "../include/solana.h"

int main()
{
    const auto rpc_url = "https://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88";
    const auto pubKey = "98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue";

    sol::Connection connection = sol::Connection{rpc_url, "finalized"};
    auto key  = sol::PublicKey::fromBase58(pubKey);
    std::optional<sol::AccountInfo> info = connection.getAccountInfo(key);
    if (!info.has_value()){
        std::cout << "Account doesn't exist.\n";
        return 0;
    }
    std::cout << "Account Owner: " << info.value().owner.toBase58() << std::endl;

    return 0;
}