cmake_minimum_required(VERSION 3.10)

# set the project name
project(solcpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Boost REQUIRED)
find_package(CURL REQUIRED)
find_package(doctest REQUIRED)
find_package(OpenSSL REQUIRED)

find_package(websocketpp CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)

include(FetchContent)

FetchContent_Declare(cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.7.2)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG v3.10.5)
FetchContent_MakeAvailable(json)

FetchContent_Declare(websocketpp
GIT_REPOSITORY https://github.com/zaphoyd/websocketpp
GIT_TAG 0.8.2)
FetchContent_MakeAvailable(websocketpp)


include_directories(SYSTEM ${Boost_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${SODIUM_INCLUDE_DIRS} ${websocketpp_INCLUDE_DIR})
add_compile_options(${SODIUM_CFLAGS_OTHER})
link_libraries( cpr::cpr ${CURL_LIBRARIES} ${OPENSSL_LIBRARIES} nlohmann_json ${SODIUM_LDFLAGS} pthread)
if(UNIX AND NOT APPLE)
  link_libraries(-L${Boost_LIBRARY_DIRS} boost_system)
endif()

# examples
add_executable(example-get-account-info examples/getAccountInfo.cpp)
add_executable(example-account-subscribe examples/accountSubscribe.cpp)
add_executable(example-send-transaction examples/sendTransaction.cpp)
add_executable(example-place-order examples/placeOrder.cpp)

# tests
include(CTest)
add_executable(tests tests/main.cpp)
add_test(NAME TestMangoGroup COMMAND tests)
