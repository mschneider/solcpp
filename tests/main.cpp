#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "mango_v3.hpp"

TEST_CASE("base58 decode & encode") {
  const std::vector<std::string> bs58s
  {
      "98pjRuQjK3qA6gXts96PqZT4Ze5QmnCmt3QYjhbUSPue",
      "mv3ekLzLbnVPNxjSKvqBpU3ZeZXPQdEC3bp5MDEBG68",
      "9xQeWvG816bUx9EPjHmaT23yvVM2ZWbrrpZb9PusVFin",
      "MangoCzJ36AjZyKwVj3VnYU4GTonjfVEnJmvvWaxLac",
      "14ivtgssEBoBjuZJtSAPKYgpUK7DmnSwuPMqJoVTSgKJ"
  };

  std::string resources_dir = FIXTURES_DIR;
  for (const auto &bs58 : bs58s) {
    const auto decoded = solana::b58decode(bs58);
    const auto encoded = solana::b58encode(decoded);
    const auto redecoded = solana::b58decode(encoded);
    std::ifstream fixture(resources_dir + "/base58/" + bs58, ios::binary);
    std::vector<char> buffer(std::istreambuf_iterator<char>(fixture), {});

    CHECK_EQ(bs58.size(), encoded.size());
    CHECK_EQ(bs58, encoded);
    CHECK_EQ(decoded.size(), redecoded.size());
    CHECK_EQ(decoded, redecoded);
    CHECK_EQ(decoded.size(), buffer.size());
    CHECK_EQ(decoded, std::string(buffer.begin(), buffer.end()));
  }
}

TEST_CASE("parse private keys") {
  std::string resources_dir = FIXTURES_DIR;
  const auto keypair =
      solana::Keypair::fromFile(resources_dir + "/solana/id.json");
  CHECK_EQ("8K4Exjnvs3ZJQDE78zmFoax5Sh4cEVdbk1D1r17Wxuud",
           keypair.publicKey.toBase58());
}

TEST_CASE("decode mango_v3 Fill") {
  const std::string encoded(
      "AAEMAAEAAAB6PABiAAAAAJMvAwAAAAAAEp7AH3xFwgByZdzdjJaK2f9K+"
      "nwfGkKL3EBs6qBSkbT0Wsj+/////3JYBgAAAAAAPNHr0H4BAADkFB3J5f//////////////"
      "AAAAAAAAAABfPABiAAAAABh0e79OvRxWYgRL9dtu02f5VK/SK/"
      "CK1oU+"
      "Tgm1NbL9IaU3AQAAAADOMAYAAAAAAAAAAAAAAAAA46WbxCAAAAAAAAAAAAAAAHJYBgAAAAAA"
      "AQAAAAAAAAA=");
  const std::string decoded = solana::b64decode(encoded);
  const mango_v3::FillEvent *event = (mango_v3::FillEvent *)decoded.data();
  CHECK_EQ(event->eventType, mango_v3::EventType::Fill);
  CHECK_EQ(event->takerSide, mango_v3::Side::Sell);
  CHECK_EQ(event->makerOut, 0);
  CHECK_EQ(event->timestamp, 1644182650);
  CHECK_EQ(event->seqNum, 208787);
  CHECK_EQ(event->maker.toBase58(),
           "2Fgjpc7bp9jpiTRKSVSsiAcexw8Cawbz7GLJu8MamS9q");
  CHECK_EQ(to_string(event->makerOrderId), "7671244543748780405054196");
  CHECK_EQ(event->makerClientOrderId, 1644182622524);
  CHECK_EQ((int)round(event->makerFee.toDouble() * 10000), -4);
  CHECK_EQ(event->bestInitial, 0);
  CHECK_EQ(event->makerTimestamp, 1644182623);
  CHECK_EQ(event->taker.toBase58(),
           "2eTob7jrhKeHNhkK1jTfS3kZYdtNQS1VF7LETom6YHjJ");
  CHECK_EQ(to_string(event->takerOrderId), "7484028538144702206551329");
  CHECK_EQ(event->takerClientOrderId, 0);
  CHECK_EQ((int)round(event->takerFee.toDouble() * 10000), 5);
  CHECK_EQ(event->price, 415858);
  CHECK_EQ(event->quantity, 1);
}

TEST_CASE("compile memo transaction") {
  const solana::PublicKey recentBlockhash = {};
  const auto feePayer = solana::PublicKey::fromBase58(
      "8K4Exjnvs3ZJQDE78zmFoax5Sh4cEVdbk1D1r17Wxuud");
  const auto memoProgram =
      solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
  const std::string memo = "Hello \xF0\x9F\xA5\xAD";
  const solana::Instruction ix = {
      memoProgram, {}, std::vector<uint8_t>(memo.begin(), memo.end())};
  const auto ctx = solana::CompiledTransaction::fromInstructions(
      {ix}, feePayer, recentBlockhash);

  CHECK_EQ(recentBlockhash, ctx.recentBlockhash);
  CHECK_EQ(2, ctx.accounts.size());
  CHECK_EQ(feePayer, ctx.accounts[0]);
  CHECK_EQ(memoProgram, ctx.accounts[1]);
  CHECK_EQ(1, ctx.instructions[0].programIdIndex);
  CHECK_EQ(0, ctx.instructions[0].accountIndices.size());
  CHECK_EQ(ix.data, ctx.instructions[0].data);
  CHECK_EQ(1, ctx.requiredSignatures);
  CHECK_EQ(0, ctx.readOnlySignedAccounts);
  CHECK_EQ(1, ctx.readOnlyUnsignedAccounts);
}
