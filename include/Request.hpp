#pragma once

namespace solana {
    class Request{
    public:
        static json fromJson(const std::string &method, const json &params = nullptr, int identifier = 1)
        {
            json req = {
                {"jsonrpc", "2.0"},
                {"id", identifier},
                {"method", method}
            };
            if (params != nullptr)
                req["params"] = params;
            return req;
        }

    };

}