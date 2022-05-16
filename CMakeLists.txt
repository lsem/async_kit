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

set(playground_main_cpp "asio_playground_main.cpp")

file(GLOB HPP_FILES "*.hpp")
file(GLOB CPP_FILES "*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_test\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_test\\.cpp")
list(FILTER CPP_FILES EXCLUDE REGEX ${playground_main_cpp})
add_executable(asio_playground ${HPP_FILES} ${CPP_FILES} ${playground_main_cpp})
target_link_libraries(asio_playground PRIVATE asio::asio_static Threads::Threads)

file(GLOB TEST_CPP_FILES "*_test.cpp")
add_executable(asio_playground_tests ${TEST_CPP_FILES} ${CPP_FILES})
target_link_libraries(asio_playground_tests PRIVATE GTest::gtest GTest::gtest_main )
