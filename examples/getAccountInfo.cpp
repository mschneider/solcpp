#include <curl/curl.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../mango_v3.hpp"

size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

int main()
{
  CURL *curl;
  CURLcode err;
  std::string readBuffer;

  curl = curl_easy_init();
  if (curl)
  {
    curl_easy_setopt(curl, CURLOPT_URL, "https://mango.rpcpool.com/946ef7337da3f5b8d3e4a34e7f88");
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const json req = solana::rpc::getAccountInfoRequest("98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue");
    const std::string jsonSerialized = req.dump();

    std::cout
        << "REQ: " << jsonSerialized << std::endl;

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonSerialized.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonSerialized.length());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    err = curl_easy_perform(curl);

    std::cout << "ERR: " << err << std::endl;

    curl_easy_cleanup(curl);

    std::cout << "RES: " << readBuffer << std::endl;

    if (!err)
    {
      json res = json::parse(readBuffer);

      const std::string encoded = res["result"]["value"]["data"][0];
      const std::string decoded = b64decode(encoded);
      const mango_v3::MangoGroup *group = reinterpret_cast<const mango_v3::MangoGroup *>(decoded.data());
      std::cout << "DEC: " << std::endl;
      std::cout << "numOracles: " << group->numOracles << std::endl;

      for (int i = 0; i < mango_v3::MAX_TOKENS; ++i)
      {
        if (i >= group->numOracles && i != mango_v3::QUOTE_INDEX)
          continue;

        const auto &token = group->tokens[i];
        const auto mintPk = token.mint.toBase58();

        if (mintPk == std::string("11111111111111111111111111111111"))
          continue;

        const auto rootBankPk = token.rootBank.toBase58();
        if (i != mango_v3::QUOTE_INDEX)
          std::cout << "TOK" << i << std::endl;
        else
          std::cout << "QUOTE" << i << std::endl;
        std::cout << "  mint: " << mintPk << std::endl;
        std::cout << "  rootBank: " << rootBankPk << std::endl;
        std::cout << "  decimals: " << (int)token.decimals << std::endl;
      }

      for (int i = 0; i < mango_v3::MAX_PAIRS; ++i)
      {
        const auto &market = group->spotMarkets[i];
        const auto marketPk = market.spotMarket.toBase58();

        if (marketPk == std::string("11111111111111111111111111111111"))
          continue;

        std::cout << "SPOT" << i << std::endl;
        std::cout << "  market: " << marketPk << std::endl;
        std::cout << "  maintAssetWeight: " << market.maintAssetWeight.toDouble() << std::endl;
        std::cout << "  initAssetWeight: " << market.initAssetWeight.toDouble() << std::endl;
        std::cout << "  maintLiabWeight: " << market.maintLiabWeight.toDouble() << std::endl;
        std::cout << "  initLiabWeight: " << market.initLiabWeight.toDouble() << std::endl;
        std::cout << "  liquidationFee: " << market.liquidationFee.toDouble() << std::endl;
      }

      for (int i = 0; i < mango_v3::MAX_PAIRS; ++i)
      {
        const auto &market = group->perpMarkets[i];
        const auto marketPk = market.perpMarket.toBase58();

        if (marketPk == std::string("11111111111111111111111111111111"))
          continue;

        std::cout << "PERP" << i << std::endl;
        std::cout << "  market: " << marketPk << std::endl;
        std::cout << "  maintAssetWeight: " << market.maintAssetWeight.toDouble() << std::endl;
        std::cout << "  initAssetWeight: " << market.initAssetWeight.toDouble() << std::endl;
        std::cout << "  maintLiabWeight: " << market.maintLiabWeight.toDouble() << std::endl;
        std::cout << "  initLiabWeight: " << market.initLiabWeight.toDouble() << std::endl;
        std::cout << "  liquidationFee: " << market.liquidationFee.toDouble() << std::endl;
        std::cout << "  makerFee: " << market.makerFee.toDouble() << std::endl;
        std::cout << "  takerFee: " << market.takerFee.toDouble() << std::endl;
        std::cout << "  baseLotSize: " << market.baseLotSize << std::endl;
        std::cout << "  quoteLotSize: " << market.quoteLotSize << std::endl;
      }

      for (int i = 0; i < mango_v3::MAX_PAIRS; ++i)
      {
        const auto oraclePk = group->oracles[i].toBase58();

        if (oraclePk == std::string("11111111111111111111111111111111"))
          continue;

        std::cout << "ORACLE" << i << ": " << oraclePk << std::endl;
      }

      std::cout << "signerNonce: " << group->signerNonce << std::endl;
      std::cout << "signerKey: " << group->signerKey.toBase58() << std::endl;
      std::cout << "admin: " << group->admin.toBase58() << std::endl;
      std::cout << "dexProgramId: " << group->dexProgramId.toBase58() << std::endl;
      std::cout << "mangoCache: " << group->mangoCache.toBase58() << std::endl;
      std::cout << "validInterval: " << group->validInterval << std::endl;
      std::cout << "insuranceVault: " << group->insuranceVault.toBase58() << std::endl;
      std::cout << "srmVault: " << group->srmVault.toBase58() << std::endl;
      std::cout << "msrmVault: " << group->msrmVault.toBase58() << std::endl;
      std::cout << "feesVault: " << group->feesVault.toBase58() << std::endl;
      std::cout << "maxMangoAccounts: " << group->maxMangoAccounts << std::endl;
      std::cout << "numMangoAccounts: " << group->numMangoAccounts << std::endl;
    }
  }
  return 0;
}