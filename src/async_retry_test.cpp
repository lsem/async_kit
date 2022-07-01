#include "async_retry.hpp"

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

using namespace lsem::async;

TEST(async_retry_tests, no_results_success_test__dispatch) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) { done_cb(std::error_code()); };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::error_code> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec) { actual_result = ec; });

    ctx.run();

    EXPECT_EQ(actual_result, std::error_code());
}

TEST(async_retry_tests, no_results_success_test__post) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) {
        ctx.post([done_cb = std::move(done_cb)] { done_cb(std::error_code()); });
    };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::error_code> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec) { actual_result = ec; });

    ctx.run();

    EXPECT_EQ(actual_result, std::error_code());
}

TEST(async_retry_tests, no_results_failure_test__dispatch) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) { done_cb(make_error_code(std::errc::io_error)); };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::error_code> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec) { actual_result = ec; });

    ctx.run();

    EXPECT_EQ(actual_result, std::errc::io_error);
}

TEST(async_retry_tests, no_results_failure_test__post) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) {
        ctx.post([done_cb = std::move(done_cb)] { done_cb(make_error_code(std::errc::io_error)); });
    };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::error_code> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec) { actual_result = ec; });

    ctx.run();

    EXPECT_EQ(actual_result, std::errc::io_error);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(async_retry_tests, with_results_success_test__dispatch) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) { done_cb(std::error_code(), 42); };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();

    EXPECT_EQ(actual_result, (std::tuple{std::error_code(), 42}));
}

TEST(async_retry_tests, with_results_success_test__post) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) {
        ctx.post([done_cb = std::move(done_cb)] { done_cb(std::error_code(), 42); });
    };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();

    EXPECT_EQ(actual_result, (std::tuple{std::error_code(), 42}));
}

TEST(async_retry_tests, with_results_failure_test__dispatch) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) { done_cb(make_error_code(std::errc::io_error), 0); };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();

    EXPECT_EQ(actual_result, (std::tuple{std::errc::io_error, 0}));
}

TEST(async_retry_tests, with_results_failure_test__post) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) {
        ctx.post([done_cb = std::move(done_cb)] { done_cb(make_error_code(std::errc::io_error), 0); });
    };

    auto async_scan_with_retry = async_retry(ctx, async_scan);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();

    EXPECT_EQ(actual_result, (std::tuple{std::errc::io_error, 0}));
}

////////////////////////////////////////////////////////////////////////

TEST(async_retry_tests, zero_pause_between_attempts_test) {
    asio::io_context ctx;

    auto async_scan = [](asio::io_context& ctx, auto done_cb) {
        ctx.post([done_cb = std::move(done_cb)] { done_cb(make_error_code(std::errc::io_error), 0); });
    };

    auto async_scan_with_retry = async_retry(ctx, async_scan, {.attempts = 1, .pause = 0s});

    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_scan_with_retry(ctx, [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();

    EXPECT_EQ(actual_result, (std::tuple{std::errc::io_error, 0}));
}
