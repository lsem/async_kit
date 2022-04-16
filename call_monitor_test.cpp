#include "call_monitor.hpp"

#include <asio/io_context.hpp>

#include <gtest/gtest.h>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

TEST(call_monitor_test, basic_test) {
    // call monitor intended to catch slow sync calls in evet-loop based concurrent programs.

    std::stringstream log;
    call_monitor::start([&](std::string s) { log << s; });

    struct stop_monitor_at_test_end {
        ~stop_monitor_at_test_end() { call_monitor::stop(); }
    } _;

    asio::io_context ctx;

    ctx.post([&] {
        call_monitor::report_hang(
            [&] {
                log << "sleep(3s).begin\n";
                std::this_thread::sleep_for(3s);
                log << "sleep(3s).end\n";
            },
            "sleep 5s", 100ms);
    });

    ctx.run();

    constexpr auto expected_log =
        "sleep(3s).begin\n"
        "CALL HANG: 'sleep 5s' >100ms\n"
        "sleep(3s).end\n";
    EXPECT_EQ(log.str(), expected_log);
}

TEST(call_monitor_test, start_with_non_started_monitor_test) {
    // for unit tests scenario call monitor is not needed and can be just not started
    // making calls to monitoring do nothing.

    std::stringstream log;

    asio::io_context ctx;

    ctx.post([&] {
        call_monitor::report_hang(
            [&] {
                log << "sleep(3s).begin\n";
                std::this_thread::sleep_for(3s);
                log << "sleep(3s).end\n";
            },
            "sleep 5s", 100ms);
    });

    ctx.run();

    constexpr auto expected_log =
        "sleep(3s).begin\n"
        "sleep(3s).end\n";
    EXPECT_EQ(log.str(), expected_log);
}