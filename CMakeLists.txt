cmake_minimum_required(VERSION 3.5)

include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.23.297.tar.gz"
    SHA1 "3319fe6a3b08090df7df98dee75134d68e2ef5a3"
)

project(asio_playground)

hunter_add_package(asio)

find_package(Threads REQUIRED)
find_package(asio CONFIG REQUIRED)


file(GLOB HPP_FILES "*.hpp")
file(GLOB CPP_FILES "*.cpp")
add_executable(asio_playground ${HPP_FILES} ${CPP_FILES})

target_link_libraries(asio_playground PRIVATE asio::asio_static Threads::Threads)

