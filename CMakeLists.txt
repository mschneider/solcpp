cmake_minimum_required(VERSION 3.10)

# set the project name
project(solcpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# lib
add_subdirectory(lib)

# examples
add_executable(example-get-account-info examples/GetAccountInfo.cpp)
add_executable(example-account-subscribe examples/AccountSubscribe.cpp)
add_executable(example-send-transaction examples/SendTransaction.cpp)
add_executable(example-place-order examples/PlaceOrder.cpp)

# link conan
target_link_libraries(example-get-account-info ${CONAN_LIBS})
target_link_libraries(example-account-subscribe ${CONAN_LIBS})
target_link_libraries(example-send-transaction ${CONAN_LIBS})
target_link_libraries(example-place-order ${CONAN_LIBS})

# libsol example
add_executable(connection-example examples/ConnectionExample.cpp)
target_link_libraries(connection-example ${CONAN_LIBS})
target_link_libraries(connection-example sol) # main libsol

# tests
include(CTest)
add_executable(tests tests/main.cpp)
target_link_libraries(tests ${CONAN_LIBS})
add_test(NAME TestMangoGroup COMMAND tests)
