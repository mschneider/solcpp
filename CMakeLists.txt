cmake_minimum_required(VERSION 3.10)

# set the project name
project(solcpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# examples
add_executable(example-get-account-info examples/getAccountInfo.cpp)
add_executable(example-account-subscribe examples/accountSubscribe.cpp)
add_executable(example-send-transaction examples/sendTransaction.cpp)
add_executable(example-place-order examples/placeOrder.cpp)
add_executable(example-ticker examples/ticker.cpp)

# link
target_link_libraries(example-get-account-info ${CONAN_LIBS})
target_link_libraries(example-account-subscribe ${CONAN_LIBS})
target_link_libraries(example-send-transaction ${CONAN_LIBS})
target_link_libraries(example-place-order ${CONAN_LIBS})
target_link_libraries(example-ticker ${CONAN_LIBS})

# tests
include(CTest)
add_executable(tests tests/main.cpp)
target_link_libraries(tests ${CONAN_LIBS})
add_test(NAME TestMangoGroup COMMAND tests)
