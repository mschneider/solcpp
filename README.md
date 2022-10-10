# solcpp - A fast Solana and Mango Markets C++ SDK.
A fast C++ SDK to interact with Solana and Mango Markets. The SDK also includes a collection of examples to send transactions to Solana and trade on Mango Markets.

If you are experienced in c++ dev and want to work on this full-time, contact [@m_schneider](https://twitter.com/m_schneider) on Twitter.

## Install
### 1. `libsol` - Solana SDK
Install as a static lib(faster compile times). See [building](#building).

See example [CMakeLists.txt](https://github.com/mschneider/solcpp/blob/main/tests/CMakeLists.txt) on how to use.

### 2. Mango Markets SDK
#### Header only
Copy the include [folder](https://github.com/mschneider/solcpp/tree/main/include) to your build tree and use a C++17 compiler.
The Mango Markets SDK depends on `libsol` above.
## Usage examples

### 1. Load and decode account info
```cpp
#include "solana.hpp"

const auto accountPubkey = "<pub key>";
const auto rpc_url = "<rpc url>";
auto connection = solana::rpc::Connection(rpc_url);
struct MyAccountInfo {
    int64_t basePosition;
    i80f48 quotePosition;
    i80f48 deposits[MAX_TOKENS];
    ...
};
MyAccountInfo myAccountInfo =
      connection.getAccountInfo<MyAccountInfo>(accountPubkey);
std::cout << myAccoutInfo.basePosition << std::endl;
```
See full [example](https://github.com/mschneider/solcpp/blob/main/examples/positions.cpp#L11).
### 2. Send Transaction
```cpp
#include "solana.hpp"

// 1. fetch recent blockhash to anchor tx to
auto recentBlockHash = connection.getLatestBlockhash();

// 2. assemble tx
const solana::PublicKey feePayer = solana::PublicKey::fromBase58(
  "<fee payer>");
const solana::PublicKey memoProgram =
  solana::PublicKey::fromBase58(solana::MEMO_PROGRAM_ID);
const std::string memo = "Hello \xF0\x9F\xA5\xAD";

const solana::CompiledInstruction ix = {
  1, {}, std::vector<uint8_t>(memo.begin(), memo.end())};

const solana::CompiledTransaction tx = {
  recentBlockHash, {feePayer, memoProgram}, {ix}, 1, 0, 1};

// 3. send & sign tx
const auto keypair =
  solana::Keypair::fromFile("id.json");
const auto b58Sig = connection.sendTransaction(keypair, tx);
spdlog::info(
  "sent tx. check: https://explorer.solana.com/tx/{}?cluster=devnet",
  b58Sig);
```
See full [example](https://github.com/mschneider/solcpp/blob/main/examples/sendTransaction.cpp).
### 3. Place Mango Perp Order
See full [example](https://github.com/mschneider/solcpp/blob/main/examples/placeOrder.cpp).
### 4. Subscribe to Account updates(fills)
See full [example](https://github.com/mschneider/solcpp/blob/main/examples/accountSubscribe.cpp).
### 5. Build complex transactions(atomic cancel and replace)
See full [example](https://github.com/mschneider/solcpp/blob/main/examples/placeOrder.cpp).
### 5. Calculate Mango Account Health
```cpp
#include "MangoAccount.hpp"

const auto& mangoAccountInfo =
  connection.getAccountInfo<mango_v3::MangoAccountInfo>(accountPubkey);
mango_v3::MangoAccount mangoAccount =
  mango_v3::MangoAccount(mangoAccountInfo);
auto openOrders = mangoAccount.loadOpenOrders(connection);
auto group = connection.getAccountInfo<mango_v3::MangoGroup>(config.group);
auto cache = connection.getAccountInfo<mango_v3::MangoCache>(
  group.mangoCache.toBase58());
auto maintHealth =
  mangoAccount.getHealth(group, cache, mango_v3::HealthType::Maint);
auto initHealth =
  mangoAccount.getHealth(group, cache, mango_v3::HealthType::Init);
auto maintHealthRatio =
  mangoAccount.getHealthRatio(group, cache, mango_v3::HealthType::Maint);
```
See full [example](https://github.com/mschneider/solcpp/blob/main/examples/positions.cpp#L7).
## Building
The project uses [conan.io](https://conan.io/) to manage dependencies. Install Conan [here](https://conan.io/downloads.html).
```sh
$ git clone https://github.com/mschneider/solcpp.git
# Create a default profile or copy over the example for linux / macos
$ conan profile new default --detect
$ cd solcpp && mkdir build && cd build
$ conan install .. --build=missing -o:h boost:without_contract=True -o:h boost:without_fiber=True -o:h boost:without_graph=True -o:h boost:without_graph_parallel=True -o:h boost:without_json=True -o:h boost:without_log=True -o:h boost:without_math=True -o:h boost:without_mpi=True -o:h boost:without_nowide=True -o:h boost:without_program_options=True -o:h boost:without_python=True -o:h boost:without_stacktrace=True -o:h boost:without_test=True -o:h boost:without_timer=True -o:h boost:without_type_erasure=True -o:h boost:without_wave=True
$ cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
$ cmake --build .
$ ./bin/tests # Run tests
$ ./bin/example-send-transaction # Run sendTransaction example
```

## Dependencies
- C++17
- boost 1.76.0 [Boost]
- cpr 1.7.2 [MIT]
- curl 7.81.0 [MIT]
- doctest 2.4.8 [MIT]
- sodium 1.0.18 [ISC]
- websocketpp 0.8.2 [BSD]
- nlohmann-json 3.10.5 [MIT]

## Notes
- _If you have issues building libcurl on gcc-9, try clang. See
  [issue](https://github.com/curl/curl/issues/4821)._

- _If you have issues linking cpr on linux gcc, try compiling with
  `libcxx=libstdc++11`. See [issue](https://github.com/libcpr/cpr/issues/125)._

- _In addition, some dependencies were directly included and slightly modified to
  work well with the rest of the code base._
    - [bitcoin/libbase58](https://github.com/bitcoin/libbase58/tree/b1dd03fa8d1be4be076bb6152325c6b5cf64f678) [MIT]
    - [base64](https://stackoverflow.com/a/37109258/18072933) [CC BY-SA 4.0]
    - [cpp-utilities/fixed](https://github.com/eteran/cpp-utilities/blob/master/fixed/include/cpp-utilities/fixed.h)
      [MIT]
## Contributing
1. Build the project locally using the [building](#building) steps above.

2. Pick one of the issues or add your feature and send in your PR :).

3. Add a unit test [here](https://github.com/mschneider/solcpp/blob/main/tests/main.cpp) to verify and guard your change.

## Documentation
Mango Markets documentation can be found [here](https://docs.mango.markets/development-resources/client-libraries).
