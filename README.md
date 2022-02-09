# solcpp

This is a simple collection of examples and dependencies to use Solana from c++.
By no means a complete SDK, yet, rather a starting point for the community to
form around and take it to the next level. If you are experienced in c++ dev and
want to work on this full-time, contact @m_schneider on twitter.

# Dependencies

These are the dependencies I installed on my development machine, while
developing, other's might work. Please report if you get it to run on earlier
versions.

- C++17
- boost 1.76.0 [Boost]
- cpr 1.7.2 [MIT]
- curl 7.81.0 [MIT]
- sodium 1.0.18 [ISC]
- websocketpp 0.8.2 [BSD]
- nlohmann-json 3.10.5 [MIT]

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

# TODO

1. Find a maintainer
2. Make a proper SDK out of this mess, this includes solving engineering issues,
   I have no idea about, like:

- proper dependency installation
- API design
- proper packaging so it can be installed

3. Improve liquidity on mango markets
