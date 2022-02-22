include_directories(${solcpp_SOURCE_DIR}/include)

# examples
add_executable(example-get-account-info getAccountInfo.cpp)
add_executable(example-account-subscribe accountSubscribe.cpp)
add_executable(example-send-transaction sendTransaction.cpp)
add_executable(example-place-order placeOrder.cpp)
add_executable(example-orderbook-subscribe orderbookSubscribe.cpp)

# link
target_link_libraries(example-get-account-info ${CONAN_LIBS} sol)
target_link_libraries(example-account-subscribe ${CONAN_LIBS} sol)
target_link_libraries(example-send-transaction ${CONAN_LIBS} sol)
target_link_libraries(example-place-order ${CONAN_LIBS} sol)
target_link_libraries(example-orderbook-subscribe ${CONAN_LIBS} sol)