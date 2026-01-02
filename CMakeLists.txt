cmake_minimum_required(VERSION 3.12)

project(asio_playground)

set (CMAKE_CXX_STANDARD 20)
####################################################################################
# OPTIONS
option(ASYNCKIT_EXTERNAL_ASIO "Let the client bring its own asio available as asio::asio" OFF)
option(ASYNCKIT_EXTERNAL_FMT "Let the client bring its own fmt available as fmt::fmt" OFF)
option(ASYNCKIT_EXTERNAL_GTEST "Let the client bring its own GTest available as [GTest::gtest, GTest::gmock, GTest::gtest_main]" OFF)

if (NOT ASYNCKIT_EXTERNAL_ASIO)
  include(FetchContent)
  FetchContent_Declare(
    asio
    GIT_REPOSITORY git@github.com:chriskohlhoff/asio.git
    GIT_TAG        231cb29bab30f82712fcd54faaea42424cc6e710# asio-1.36.0
  )
  FetchContent_MakeAvailable(asio)
  add_library(asio INTERFACE)
  add_library(asio::asio ALIAS asio)
  target_include_directories(asio INTERFACE ${CMAKE_BINARY_DIR}/_deps/asio-src/asio/include)
endif()

if (NOT ASYNCKIT_EXTERNAL_FMT)
  include(FetchContent)
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY git@github.com:fmtlib/fmt.git
    GIT_TAG        407c905e45ad75fc29bf0f9bb7c5c2fd3475976f# 12.1.0
  )
  FetchContent_MakeAvailable(fmt)
endif()

if (NOT ASYNCKIT_EXTERNAL_GTEST)
  include(FetchContent)
  FetchContent_Declare(
    GTest
    GIT_REPOSITORY git@github.com:google/googletest.git
    GIT_TAG        52eb8108c5bdec04579160ae17225d66034bd723# 1.17.0
  )
  FetchContent_MakeAvailable(GTest)
endif()

add_subdirectory(function2)

set(SOURCES
  src/async_callback.cpp
  src/async_callback.hpp
  src/async_criticial_section.hpp
  src/async_retry.hpp
  src/async_timeoutable.hpp
  src/bounded_async_foreach.hpp
  src/call_monitor.cpp
  src/call_monitor.hpp
  src/ordered_async_ops.hpp
  src/utils.hpp
)

set(TESTS
  src/async_callback_test.cpp  
  src/async_criticial_section_test.cpp
  src/bounded_async_foreach_test.cpp
  src/ordered_async_ops_test.cpp
  src/call_monitor_test.cpp
  src/async_timeoutable_test.cpp
  src/async_retry_test.cpp  
)

add_library(async_kit ${SOURCES})
add_library(async_kit::async_kit ALIAS async_kit)
target_include_directories(async_kit PUBLIC include)
target_link_libraries(async_kit PUBLIC function2::function2 asio::asio fmt::fmt)


add_executable(async_kit_playground ${SOURCES} "async_kit_playground_main.cpp")
target_include_directories(async_kit_playground PRIVATE "src")
target_link_libraries(async_kit_playground PRIVATE pthread PUBLIC asio::asio function2::function2 fmt::fmt)

add_executable(async_kit_tests ${SOURCES} ${TESTS})
target_include_directories(async_kit_tests PRIVATE "src")
target_link_libraries(async_kit_tests PRIVATE GTest::gtest GTest::gmock GTest::gtest_main function2::function2 asio::asio fmt::fmt)

target_compile_options(async_kit_tests PRIVATE "-fsanitize=address")
target_link_options(async_kit_tests PRIVATE "-fsanitize=address")

