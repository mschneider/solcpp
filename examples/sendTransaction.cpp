#include <chrono>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../include/solana.hpp"

size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

int main()
{
  // 1. fetch recent blockhash to anchor tx to
  const json req = solana::rpc::getRecentBlockhashRequest();
  const std::string jsonSerialized = req.dump();
  std::cout << "REQ: " << jsonSerialized << std::endl;

  cpr::Response r = cpr::Post(cpr::Url{"https://mango.devnet.rpcpool.com"},
                              cpr::Body{jsonSerialized},
                              cpr::Header{{"Content-Type", "application/json"}});
  std::cout << "RES: " << r.text << std::endl;

  json res = json::parse(r.text);

  const std::string encoded = res["result"]["value"]["blockhash"];
  const solana::PublicKey recentBlockHash = solana::PublicKey::fromBase58(encoded);

  // 2. assemble tx
  const solana::PublicKey feePayer = solana::PublicKey::fromBase58("8K4Exjnvs3ZJQDE78zmFoax5Sh4cEVdbk1D1r17Wxuud");
  const solana::PublicKey memoProgram = solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
  const std::string memo = "Hello \xF0\x9F\xA5\xAD";

  const solana::CompiledInstruction ix = {
      1, {}, std::vector<uint8_t>(memo.begin(), memo.end())};

  const solana::CompiledTransaction tx = {
      recentBlockHash,
      {feePayer, memoProgram},
      {ix},
      1,
      0,
      1};

  // 3. send & sign tx
  const auto keypair = solana::Keypair::fromFile("../tests/fixtures/solana/id.json");
  const auto b58Sig = solana::rpc::signAndSendTransaction("https://mango.devnet.rpcpool.com", keypair, tx);
  std::cout << "sent tx. check: https://explorer.solana.com/tx/" << b58Sig << "?cluster=devnet" << std::endl;

  // 4. wait for tx to confirm
  const auto start = std::chrono::system_clock::now();
  while (true)
  {
    const auto sinceStart = std::chrono::system_clock::now() - start;
    const auto secondsSinceStart = std::chrono::duration_cast<std::chrono::seconds>(sinceStart).count();
    if (secondsSinceStart > 90)
      throw std::runtime_error("timeout reached - could not confirm transaction " + b58Sig);

    const json req = solana::rpc::getSignatureStatuses({b58Sig});
    const std::string jsonSerialized = req.dump();
    std::cout << "REQ: " << jsonSerialized << std::endl;

    cpr::Response r = cpr::Post(cpr::Url{"https://mango.devnet.rpcpool.com"},
                                cpr::Body{jsonSerialized},
                                cpr::Header{{"Content-Type", "application/json"}});
    std::cout << "RES: " << r.text << std::endl;
    json res = json::parse(r.text);

    if (res["result"]["value"][0] != nullptr)
      break;
  }

  return 0;
}