#include "call_monitor.hpp"

#include "ordered_async_ops.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <gtest/gtest.h>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::move(cb)](std::error_code ec) { cb(ec); });
}

TEST(ordered_async_ops_tests, basic_test) {
    // This may happen when we send some async request and have some async response handler and to avoid races
    // we first must run receive and only then, in parallel - send. But not we have a problem that receive comes before
    // send while this is not what we what. In this case we can fix reordering problem be reeranging callbacks like we
    // want.
    asio::io_context ctx;

    std::vector<std::string> log;

    ordered_async_ops ordered(ctx);

    // this is logically op2
    async_sleep(ctx, 1s, ordered(2, [&](std::error_code ec) { log.push_back("RECEIVE handler"); }));

    // and this is logically op1
    async_sleep(ctx, 2s, ordered(1, [&](std::error_code ec) { log.push_back("SEND handler"); }));

    ctx.run();

    EXPECT_EQ(log, std::vector<std::string>({"SEND handler", "RECEIVE handler"}));
}

TEST(ordered_async_ops_tests, three_processes_test) {
    // This may happen when we send some async request and have some async response handler and to avoid races
    // we first must run receive and only then, in parallel - send. But not we have a problem that receive comes before
    // send while this is not what we what. In this case we can fix reordering problem be reeranging callbacks like we
    // want.
    asio::io_context ctx;

    std::vector<std::string> log;

    ordered_async_ops ordered(ctx);

    async_sleep(ctx, 3s, ordered(1, [&](std::error_code ec) { log.push_back("FIRST handler"); }));
    async_sleep(ctx, 2s, ordered(2, [&](std::error_code ec) { log.push_back("SECOND handler"); }));
    async_sleep(ctx, 1s, ordered(3, [&](std::error_code ec) { log.push_back("THIRD handler"); }));

    ctx.run();

    EXPECT_EQ(log, std::vector<std::string>({"FIRST handler", "SECOND handler", "THIRD handler"}));
}

TEST(ordered_async_ops_tests, instance_of_oredere_can_be_removed_test) {
    asio::io_context ctx;

    std::vector<std::string> log;

    {
        ordered_async_ops ordered(ctx);
        async_sleep(ctx, 3s, ordered(1, [&](std::error_code ec) { log.push_back("FIRST handler"); }));
        async_sleep(ctx, 2s, ordered(2, [&](std::error_code ec) { log.push_back("SECOND handler"); }));
        async_sleep(ctx, 1s, ordered(3, [&](std::error_code ec) { log.push_back("THIRD handler"); }));
    }

    ctx.run();

    EXPECT_EQ(log, std::vector<std::string>({"FIRST handler", "SECOND handler", "THIRD handler"}));
}
