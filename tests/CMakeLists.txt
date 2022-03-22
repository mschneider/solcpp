include_directories(${solcpp_SOURCE_DIR}/include)

# tests
add_executable(tests main.cpp)
target_link_libraries(tests ${CONAN_LIBS})
target_compile_definitions(tests PUBLIC FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
add_test(NAME TestMangoGroup COMMAND tests)