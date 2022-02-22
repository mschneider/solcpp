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
add_subdirectory(examples)

# tests
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
