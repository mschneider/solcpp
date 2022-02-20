include_directories(${solcpp_SOURCE_DIR}/include)

# examples
add_executable(example-get-account-info getAccountInfo.cpp)
add_executable(example-account-subscribe accountSubscribe.cpp)
add_executable(example-send-transaction sendTransaction.cpp)
add_executable(example-place-order placeOrder.cpp)

# link conan
target_link_libraries(example-get-account-info ${CONAN_LIBS})
target_link_libraries(example-account-subscribe ${CONAN_LIBS})
target_link_libraries(example-send-transaction ${CONAN_LIBS})
target_link_libraries(example-place-order ${CONAN_LIBS})

# link solana
target_link_libraries(example-get-account-info sol)
target_link_libraries(example-account-subscribe sol)
target_link_libraries(example-send-transaction sol)
target_link_libraries(example-place-order sol)