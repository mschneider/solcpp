# solcpp

A simple collection of examples and dependencies to use Solana from C++. By no
means a complete SDK, yet, rather a starting point for the community to form
around and take it to the next level. If you are experienced in c++ dev and want
to work on this full-time, contact @m_schneider on twitter.

# Dependencies

The project uses [conan.io](https://conan.io/) to manage the following
dependencies. Install Conan [here](https://conan.io/downloads.html).

- C++17
- boost 1.76.0 [Boost]
- cpr 1.7.2 [MIT]
- curl 7.81.0 [MIT]
- doctest 2.4.8 [MIT]
- sodium 1.0.18 [ISC]
- websocketpp 0.8.2 [BSD]
- nlohmann-json 3.10.5 [MIT]

## Building:

```sh
# Create a default profile or copy over the example for linux / macos
$ conan profile new default --detect
$ mkdir build && cd build
$ conan install ..  \
  --build=missing \
  -o:h boost:without_fiber=True \ # Skips building boost's header-only fiber
  -o:h boost:without_python=True \ # Skips python bindings
$ cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
$ cmake --build .
$ ./bin/tests
```

_Note: if you have issues building libcurl on gcc-9, try clang. See
[issue](https://github.com/curl/curl/issues/4821)._

_Note: if you have issues linking cpr on linux gcc, try compiling with
`libcxx=libstdc++11`. See [issue](https://github.com/libcpr/cpr/issues/125)._

In addition some dependencies were directly included and slighly modified to
work well with the rest of the code base.

- bitcoin/libbase58 (b1dd03fa8d1be4be076bb6152325c6b5cf64f678) [MIT]
- base64 (https://stackoverflow.com/a/37109258/18072933) [CC BY-SA 4.0]
- fixedp
  (https://gist.github.com/dflemstr/294959/aa90ff5b1a66b45b9edb30a432a66f8383d368e6)
  [CC BY-SA 3.0]

# Examples

- Load & decode AccountInfo
- Subscribe to account updates (fills)
- Send Transaction
- Build complex transactions (atomic cancel and replace)
- MangoAccount Health info and calculations

## TODO

- [ ] Find a maintainer
- [ ] Make a proper SDK out of this mess, this includes solving engineering
      issues, I have no idea about, like:

- API design
- packaging so it can be easily installed

- [ ] Improve liquidity on mango markets
- [ ] Remove/replace deprecated methods
- [ ] Remove the pragma warnings
