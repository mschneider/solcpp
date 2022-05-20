#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <future>
#include <nlohmann/json.hpp>

#include "mango_v3.hpp"
#include "solana.hpp"

using json = nlohmann::json;

int main() {
  std::string rpc_url = "https://mango.devnet.rpcpool.com";
  auto connection = solana::rpc::Connection(rpc_url);
  // 1. fetch recent blockhash to anchor tx to
  auto recentBlockHash = connection.getLatestBlockhash();

  // 2. assemble tx
  const solana::PublicKey feePayer = solana::PublicKey::fromBase58(
      "8K4Exjnvs3ZJQDE78zmFoax5Sh4cEVdbk1D1r17Wxuud");
  const solana::PublicKey memoProgram =
      solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
  const std::string memo = "Hello \xF0\x9F\xA5\xAD";

  const solana::CompiledInstruction ix = {
      1, {}, std::vector<uint8_t>(memo.begin(), memo.end())};

  const solana::CompiledTransaction tx = {
      recentBlockHash, {feePayer, memoProgram}, {ix}, 1, 0, 1};

  // 3. send & sign tx
  const auto keypair =
      solana::Keypair::fromFile("../tests/fixtures/solana/id.json");
  const auto b58Sig = connection.signAndSendTransaction(keypair, tx);
  spdlog::info(
      "sent tx. check: https://explorer.solana.com/tx/{}?cluster=devnet",
      b58Sig);

  // 4. wait for tx to confirm
  const auto timeoutBlockHeight =
      recentBlockHash.lastValidBlockHeight +
      mango_v3::MAXIMUM_NUMBER_OF_BLOCKS_FOR_TRANSACTION;
  while (true) {
    // Asynchronously fetch blockHeight
    std::future<uint64_t> blockHeightFut =
        std::async(std::launch::async, [&connection]() -> uint64_t {
          return connection.getBlockHeight("confirmed");
        });
    // Check if we are past validBlockHeight
    auto currentBlockHeight = blockHeightFut.get();
    if (timeoutBlockHeight <= currentBlockHeight) {
      spdlog::error("Timed out for txid: {}, current BlockHeight: {}", b58Sig,
                    currentBlockHeight);
      return EXIT_FAILURE;
    }

    const json req = connection.getSignatureStatuses({b58Sig});
    const std::string jsonSerialized = req.dump();
    spdlog::info("REQ: {}", jsonSerialized);

    cpr::Response r =
        cpr::Post(cpr::Url{rpc_url}, cpr::Body{jsonSerialized},
                  cpr::Header{{"Content-Type", "application/json"}});
    if (r.status_code == 0 || r.status_code >= 400) {
      spdlog::error("Error: {}, {}", r.status_code, r.error.message);
      return EXIT_FAILURE;
    } else {
      spdlog::info("RES: {}", r.text);
      json res = json::parse(r.text);

      if (res["result"]["value"][0] != nullptr) break;
    }
  }
  return EXIT_SUCCESS;
}
