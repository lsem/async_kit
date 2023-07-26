cmake_minimum_required(VERSION 3.5)

project(asio_playground)

set (CMAKE_CXX_STANDARD 17)

# this is temporary
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_subdirectory(function2)

file(GLOB_RECURSE HPP_FILES "src/*.hpp")
file(GLOB_RECURSE CPP_FILES "src/*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_test\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_test\\.cpp")

add_library(async_kit ${HPP_FILES} ${CPP_FILES})
add_library(async_kit::async_kit ALIAS async_kit)
target_include_directories(async_kit PUBLIC include)
target_link_libraries(async_kit PUBLIC function2::function2 asio::asio)


add_executable(async_kit_playground ${HPP_FILES} ${CPP_FILES} "async_kit_playground_main.cpp")
target_include_directories(async_kit_playground PRIVATE "src")
target_link_libraries(async_kit_playground PRIVATE pthread PUBLIC asio::asio function2::function2)

file(GLOB_RECURSE TEST_CPP_FILES "src/*_test.cpp")
file(GLOB_RECURSE TEST_HPP_FILES "src/*_test.hpp")
add_executable(async_kit_tests ${TEST_CPP_FILES} ${TEST_HPP_FILES} ${CPP_FILES})
target_include_directories(async_kit_tests PRIVATE "src")
target_link_libraries(async_kit_tests PRIVATE GTest::gtest GTest::gmock GTest::gtest_main function2::function2 asio::asio)
