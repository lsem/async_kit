#include "async_timeoutable.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <gtest/gtest.h>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace {
template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::forward<Callback>(cb)](std::error_code ec) { cb(); });
}
}  // namespace

TEST(async_timeoutable_tests, with_results_test) {
    asio::io_context ctx;

    // assuming we have some async operation wifi_scan_async

    auto wifi_scan_async = [&ctx](auto scan_done) {
        async_sleep(ctx, 1s, [scan_done] {
            std::vector<std::string> fake_scan_results;
            fake_scan_results.emplace_back("ap 0");
            fake_scan_results.emplace_back("ap 1");
            scan_done(std::error_code(), fake_scan_results);
        });
    };

    std::optional<std::error_code> wifi_scan_async_result;
    wifi_scan_async([&](std::error_code ec, const std::vector<std::string>& scan_results) {
        wifi_scan_async_result = ec;
        ASSERT_FALSE(ec);
        ASSERT_EQ(scan_results.size(), 2);
    });

    // We would like to make it possible to use timeout
    auto wifi_scan_async_with_timeout = async_timeoutable(ctx, wifi_scan_async);

    std::optional<std::error_code> wifi_scan_async_with_timeout_result;
    wifi_scan_async_with_timeout(10s, [&](std::error_code ec, const std::vector<std::string>& scan_results) {
        wifi_scan_async_with_timeout_result = ec;
        ASSERT_FALSE(ec);
        ASSERT_EQ(scan_results.size(), 2);
    });

    std::optional<std::error_code> wifi_scan_async_with_timeout_result2;
    wifi_scan_async_with_timeout(100ms, [&](std::error_code ec, const std::vector<std::string>& scan_results) {
        wifi_scan_async_with_timeout_result2 = ec;
    });

    ctx.run();

    EXPECT_EQ(wifi_scan_async_result, std::error_code());
    EXPECT_EQ(wifi_scan_async_with_timeout_result, std::error_code());
    EXPECT_EQ(wifi_scan_async_with_timeout_result2, make_error_code(std::errc::timed_out));
}

TEST(async_timeoutable_tests, without_results_test) {
    asio::io_context ctx;

    // assuming we have some async operation wifi_scan_async

    auto wifi_scan_async = [&ctx](auto scan_done) {
        async_sleep(ctx, 1s, [scan_done] { scan_done(std::error_code()); });
    };

    std::optional<std::error_code> wifi_scan_async_result;
    wifi_scan_async([&](std::error_code ec) {
        wifi_scan_async_result = ec;
        ASSERT_FALSE(ec);
    });

    // We would like to make it possible to use timeout
    auto wifi_scan_async_with_timeout = async_timeoutable(ctx, wifi_scan_async);

    std::optional<std::error_code> wifi_scan_async_with_timeout_result;
    wifi_scan_async_with_timeout(10s, [&](std::error_code ec) {
        wifi_scan_async_with_timeout_result = ec;
        ASSERT_FALSE(ec);
    });

    std::optional<std::error_code> wifi_scan_async_with_timeout_result2;
    wifi_scan_async_with_timeout(100ms, [&](std::error_code ec) { wifi_scan_async_with_timeout_result2 = ec; });

    ctx.run();

    EXPECT_EQ(wifi_scan_async_result, std::error_code());
    EXPECT_EQ(wifi_scan_async_with_timeout_result, std::error_code());
    EXPECT_EQ(wifi_scan_async_with_timeout_result2, make_error_code(std::errc::timed_out));
}
