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
        call_monitor::sync_call(
            [&] {
                log << "slow_sync_call/begin\n";
                std::this_thread::sleep_for(3s);
                log << "slow_sync_call/end\n";
            },
            "slow_sync_call", 100ms);
    });

    ctx.run();

    constexpr auto expected_log =
        "slow_sync_call/begin\n"
        "CALL HANG: 'slow_sync_call' >100ms\n"
        "slow_sync_call/end\n";
    EXPECT_EQ(log.str(), expected_log);
}

TEST(call_monitor_test, start_with_non_started_monitor_test) {
    // for unit tests scenario call monitor is not needed and can be just not started
    // making calls to monitoring do nothing.

    std::stringstream log;

    asio::io_context ctx;

    ctx.post([&] {
        call_monitor::sync_call(
            [&] {
                log << "slow_sync_call/begin\n";
                std::this_thread::sleep_for(3s);
                log << "slow_sync_call/end\n";
            },
            "slow_sync_call", 100ms);
    });

    ctx.run();

    constexpr auto expected_log =
        "slow_sync_call/begin\n"
        "slow_sync_call/end\n";
    EXPECT_EQ(log.str(), expected_log);
}