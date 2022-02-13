#pragma once

#include <nlohmann/json.hpp>
namespace sol {
    using json = nlohmann::json;
    using BlockHash = std::string;
}