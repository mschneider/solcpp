include_directories(${solcpp_SOURCE_DIR}/include)
 
# examples
add_executable(example-get-account-info getAccountInfo.cpp)
add_executable(example-account-subscribe accountSubscribe.cpp)
add_executable(example-send-transaction sendTransaction.cpp)
add_executable(example-positions positions.cpp)
add_executable(example-ws ws.cpp)

# link
target_link_libraries(example-get-account-info ${CONAN_LIBS} sol)
target_link_libraries(example-account-subscribe ${CONAN_LIBS} sol)
target_link_libraries(example-send-transaction ${CONAN_LIBS} sol)
target_link_libraries(example-positions ${CONAN_LIBS} sol)
target_link_libraries(example-ws ${CONAN_LIBS} sol)