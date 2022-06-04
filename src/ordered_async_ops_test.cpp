#include "call_monitor.hpp"

#include "ordered_async_ops.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <gtest/gtest.h>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

template <typename Callback>
auto async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::move(cb)](std::error_code ec) { cb(ec); });
    return [timer]() { timer->cancel(); };
}

template <typename Callback>
void run_peridical(Callback&& cb, asio::io_context& ctx, std::chrono::steady_clock::duration t, int n) {
    if (n == 0) {
        return;
    }
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([&ctx, t, n, timer, cb = std::move(cb)](std::error_code ec) {
        cb(ec);
        run_peridical(std::move(cb), ctx, t, n - 1);
    });
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

    std::vector<std::tuple<std::string, std::error_code>> log;

    ordered_async_ops ordered(ctx);

    async_sleep(ctx, 3s, ordered(1, [&](std::error_code ec) { log.emplace_back("FIRST handler", ec); }));
    auto t2_cancel =
        async_sleep(ctx, 2s, ordered(2, [&](std::error_code ec) { log.emplace_back("SECOND handler", ec); }));
    async_sleep(ctx, 1s, ordered(3, [&](std::error_code ec) { log.emplace_back("THIRD handler", ec); }));

    t2_cancel();

    ctx.run();

    using t = std::vector<std::tuple<std::string, std::error_code>>;
    EXPECT_EQ(log, t({std::tuple{"FIRST handler", std::error_code()}, std::tuple{"SECOND handler", std::error_code()},
                      std::tuple{"THIRD handler", std::error_code()}}));
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

TEST(ordered_async_ops_tests, multiple_trailing_intances) {
    // in this scenario, second logical handler can be called multiple times.
    asio::io_context ctx;

    std::vector<std::string> log;

    {
        ordered_async_ops ordered(ctx);
        async_sleep(ctx, 3s, ordered(1, [&](std::error_code ec) { log.push_back("FIRST handler"); }));
        run_peridical(ordered(2, [&](std::error_code ec) { log.push_back("SECOND handler"); }), ctx, 2s, 3);
    }

    ctx.run();

    EXPECT_EQ(log, std::vector<std::string>({"FIRST handler", "SECOND handler", "SECOND handler"}));
}
