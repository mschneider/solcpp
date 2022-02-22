include_directories(${solcpp_SOURCE_DIR}/include)
add_library(sol solana.cpp)
target_link_libraries(sol ${CONAN_LIBS})
