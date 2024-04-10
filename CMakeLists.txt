cmake_minimum_required(VERSION 3.5)

project(asio_playground)

set (CMAKE_CXX_STANDARD 20)

add_subdirectory(function2)

file(GLOB_RECURSE HPP_FILES "src/*.hpp")
file(GLOB_RECURSE CPP_FILES "src/*.cpp")
list(FILTER HPP_FILES EXCLUDE REGEX "_test\\.hpp")
list(FILTER CPP_FILES EXCLUDE REGEX "_test\\.cpp")

add_library(async_kit ${HPP_FILES} ${CPP_FILES})
add_library(async_kit::async_kit ALIAS async_kit)
target_include_directories(async_kit PUBLIC include)
target_link_libraries(async_kit PUBLIC function2::function2 asio::asio fmt::fmt)


add_executable(async_kit_playground ${HPP_FILES} ${CPP_FILES} "async_kit_playground_main.cpp")
target_include_directories(async_kit_playground PRIVATE "src")
target_link_libraries(async_kit_playground PRIVATE pthread PUBLIC asio::asio function2::function2 fmt::fmt)

file(GLOB_RECURSE TEST_CPP_FILES "src/*_test.cpp")
file(GLOB_RECURSE TEST_HPP_FILES "src/*_test.hpp")
add_executable(async_kit_tests ${TEST_CPP_FILES} ${TEST_HPP_FILES} ${CPP_FILES})
target_include_directories(async_kit_tests PRIVATE "src")
target_link_libraries(async_kit_tests PRIVATE GTest::gtest GTest::gmock GTest::gtest_main function2::function2 asio::asio fmt::fmt)

target_compile_options(async_kit_tests PRIVATE "-fsanitize=address")
target_link_options(async_kit_tests PRIVATE "-fsanitize=address")

# Lets try to iterate filess in include directory
file(GLOB PUBLIC_HPP_FILES CONFIGURE_DEPENDS "include/async_kit/*.hpp")
set(PUBLIC_HPP_FILES_IN_SRC "")
message("PUBLIC_HPP_FILES: ${PUBLIC_HPP_FILES}")
foreach(file_path ${PUBLIC_HPP_FILES})
  cmake_path(GET file_path FILENAME file_name)
  set(header_path_in_src src/${file_name})
  message("file: " ${file_name} -> ${header_path_in_src})
  list(APPEND PUBLIC_HPP_FILES_IN_SRC ${header_path_in_src})
endforeach()
message("Resulting list: ${PUBLIC_HPP_FILES_IN_SRC}")

install(FILES ${PUBLIC_HPP_FILES_IN_SRC} DESTINATION include/async_kit)
#install(DIRECTORY include/async_kit DESTINATION include/)


# message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")

# execute_process(COMMAND ${CMAKE_CXX_COMPILER} expected_failure.cpp
#                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#                 RESULT_VARIABLE cxx_exit_code
#                 )
# if (cxx_exit_code AND NOT cxx_exit_code EQUAL 0)
#   message(STATUS ">> FAILED")
# else()
#   message(STATUS ">> SUCCESS")
# endif()

# execute_process(COMMAND ${CMAKE_CXX_COMPILER} expected_success.cpp
#                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#                 RESULT_VARIABLE cxx_exit_code
#                 )
# if (cxx_exit_code AND NOT cxx_exit_code EQUAL 0)
#   message(STATUS ">> FAILED")
# else()
#   message(STATUS ">> SUCCESS")
# endif()
