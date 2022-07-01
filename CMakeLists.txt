cmake_minimum_required(VERSION 3.5)

include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.23.297.tar.gz"
    SHA1 "3319fe6a3b08090df7df98dee75134d68e2ef5a3"
)

project(asio_playground)

hunter_add_package(asio)
hunter_add_package(GTest)

find_package(Threads REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

set (CMAKE_CXX_STANDARD 17)

# this is temporary
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_subdirectory(function2)

file(GLOB HPP_FILES "src/*.hpp")
file(GLOB CPP_FILES "src/*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_test\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_test\\.cpp")

add_executable(async_kit_playground ${HPP_FILES} ${CPP_FILES} "async_kit_playground_main.cpp")
target_include_directories(async_kit_playground PRIVATE "src")
target_link_libraries(async_kit_playground PRIVATE asio::asio_static Threads::Threads function2::function2)

file(GLOB TEST_CPP_FILES "src/*_test.cpp")
file(GLOB TEST_HPP_FILES "src/*_test.hpp")
add_executable(async_kit_tests ${TEST_CPP_FILES} ${TEST_HPP_FILES} ${CPP_FILES})
target_include_directories(async_kit_tests PRIVATE "src")
target_link_libraries(async_kit_tests PRIVATE GTest::gtest GTest::gtest_main function2::function2)
