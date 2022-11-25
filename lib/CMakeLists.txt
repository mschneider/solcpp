include_directories(${solcpp_SOURCE_DIR}/include)
add_library(websocket websocket.cpp)
add_library(sol solana.cpp)
target_link_libraries(sol websocket ${CONAN_LIBS})
