# solcpp

A simple collection of examples and dependencies to use Solana from C++.
By no means a complete SDK, yet, rather a starting point for the community to
form around and take it to the next level. If you are experienced in c++ dev and
want to work on this full-time, contact @m_schneider on twitter.

# Dependencies

The project uses [conan.io](https://conan.io/) to manage the following dependencies. Install Conan [here](https://conan.io/downloads.html).

- C++17
- boost 1.76.0 [Boost]
- cpr 1.7.2 [MIT]
- curl 7.81.0 [MIT]
- sodium 1.0.18 [ISC]
- websocketpp 0.8.2 [BSD]
- nlohmann-json 3.10.5 [MIT]

## Building:
```sh
$ conan profile new default --detect # Create a default profile
$ mkdir build && cd build
$ conan install .. --build=missing -o:h boost:without_fiber=True # Skips building boost's header-only fiber
$ cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
$ cmake --build .
```

In addition some dependencies were directly included and slighly modified to
work well with the rest of the code base.

- bitcoin/libbase58 (b1dd03fa8d1be4be076bb6152325c6b5cf64f678) [MIT]
- base64 (https://stackoverflow.com/a/37109258/18072933) [CC BY-SA 4.0]
- fixedp
  (https://gist.github.com/dflemstr/294959/aa90ff5b1a66b45b9edb30a432a66f8383d368e6)
  [CC BY-SA 3.0]

*Note: if you have issues building libcurl on gcc-9, try clang. See [issue](https://github.com/curl/curl/issues/4821).*

# Examples

- Load & decode AccountInfo
- Subscribe to account updates (fills)
- Send Transaction
- Build complex transactions (atomic cancel and replace)

# TODO

[ ] Find a maintainer

[ ] Make a proper SDK out of this mess, this includes solving engineering issues,
   I have no idea about, like:

- proper dependency installation
- API design
- proper packaging so it can be installed

[ ] Improve liquidity on mango markets

[ ] Remove/replace deprecated methods

[ ] Remove the pragma warnings

