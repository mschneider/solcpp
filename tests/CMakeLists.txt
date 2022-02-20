include_directories(${solcpp_SOURCE_DIR}/include)

# tests
include(CTest)
add_executable(tests main.cpp)
target_link_libraries(tests ${CONAN_LIBS})
add_test(NAME TestMangoGroup COMMAND tests)