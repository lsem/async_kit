#include "async_retry.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <gmock/gmock.h>
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

// test that arguments are properly forwarded (e.g. passing lvalue does not devastate passed value, test that moved
// value works)

TEST(async_retry_tests, args_are_forwarded_properly_1) {
    asio::io_context ctx;

    // async op with argument that is non-copyable and passed by const reference
    auto async_op = [](const std::unique_ptr<int>& unique_arg, int arg, auto done_cb) {
        done_cb(std::error_code(), *unique_arg + arg);
    };
    auto async_op_with_retry = async_retry(ctx, async_op);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    auto unique_arg = std::make_unique<int>(42);
    async_op_with_retry(unique_arg, 42, [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();

    EXPECT_EQ(actual_result, (std::tuple{std::error_code(), 84}));
}

TEST(async_retry_tests, args_are_forwarded_properly_2) {
    asio::io_context ctx;

    // async op with argument that is non-copyable and passed by const reference
    auto async_op = [](std::unique_ptr<int> unique_arg, auto done_cb) { done_cb(std::error_code(), *unique_arg); };
    auto async_op_with_retry = async_retry(ctx, async_op);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    auto unique_arg = std::make_unique<int>(42);
    async_op_with_retry(std::move(unique_arg), [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();

    EXPECT_EQ(actual_result, (std::tuple{std::error_code(), 42}));
}

TEST(async_retry_tests, args_are_forwarded_properly__no_result) {
    asio::io_context ctx;

    auto async_op = [](const std::unique_ptr<int>& unique_arg, int arg, auto done_cb) {
        if (*unique_arg == 10 && arg == 20) {
            done_cb(std::error_code());
        } else {
            done_cb(make_error_code(std::errc::io_error));
        }
    };
    auto async_op_with_retry = async_retry(ctx, async_op);

    std::optional<std::error_code> actual_result;
    auto unique_arg = std::make_unique<int>(10);
    async_op_with_retry(unique_arg, 20, [&](std::error_code ec) { actual_result = ec; });

    ctx.run();

    EXPECT_EQ(actual_result, std::error_code());
}

// TEST: should work with non-copyable handler (e.g. fu2::unique_function)
TEST(async_retry_tests, works_with_unique_handler) {
    asio::io_context ctx;

    auto async_op = [](asio::io_context& ctx, auto done_cb) { done_cb(std::error_code(), 42); };
    auto async_op_with_retry = async_retry(ctx, async_op);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_op_with_retry(ctx, [&, unique_member = std::make_unique<int>(7)](std::error_code ec, int r) {
        actual_result = std::tuple{ec, r};
    });

    ctx.run();
    EXPECT_EQ(actual_result, (std::tuple{std::error_code(), 42}));
}

TEST(async_retry_tests, works_with_unique_handler__no_result) {
    asio::io_context ctx;

    auto async_op = [](asio::io_context& ctx, auto done_cb) { done_cb(std::error_code()); };
    auto async_op_with_retry = async_retry(ctx, async_op);

    std::optional<std::error_code> actual_result;
    async_op_with_retry(ctx, [&, unique_member = std::make_unique<int>(7)](std::error_code ec) { actual_result = ec; });

    ctx.run();
    EXPECT_EQ(actual_result, std::error_code());
}

TEST(async_retry_tests, result_is_moved_properly) {
    asio::io_context ctx;

    // async op returning unique_ptr as result.
    auto async_op = [](asio::io_context& ctx, auto done_cb) { done_cb(std::error_code(), std::make_unique<int>(42)); };
    auto async_op_with_retry = async_retry(ctx, async_op);

    std::optional<std::tuple<std::error_code, std::unique_ptr<int>>> actual_result;
    async_op_with_retry(ctx, [&](std::error_code ec, std::unique_ptr<int> r) {
        actual_result = std::tuple{ec, std::move(r)};
    });

    ctx.run();
    ASSERT_TRUE(actual_result.has_value());
    EXPECT_EQ(*std::get<1>(*actual_result), 42);
}

TEST(async_retry_tests, options_are_respected__one_attempt) {
    asio::io_context ctx;

    // async op returning unique_ptr as result.
    int called_times = 0;
    auto async_op = [&called_times](asio::io_context& ctx, auto done_cb) {
        called_times++;
        done_cb(make_error_code(std::errc::io_error), 0);
    };

    auto async_op_with_retry = async_retry(ctx, async_op, {.attempts = 1});

    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_op_with_retry(ctx, [&](std::error_code ec, int r) { actual_result = std::tuple{ec, r}; });

    ctx.run();
    ASSERT_TRUE(actual_result.has_value());
    EXPECT_EQ(std::get<0>(actual_result.value()), make_error_code(std::errc::io_error));
    EXPECT_EQ(called_times, 1);
}

TEST(async_retry_tests, options_are_respected__five_attempts_with_custom_pause) {
    asio::io_context ctx;

    // async op returning unique_ptr as result.
    int called_times = 0;
    auto async_op = [&called_times](asio::io_context& ctx, auto done_cb) {
        called_times++;
        done_cb(make_error_code(std::errc::io_error), 0);
    };

    auto async_op_with_retry = async_retry(ctx, async_op, {.attempts = 5, .pause = 100ms});

    auto start_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration time_taken;
    std::optional<std::tuple<std::error_code, int>> actual_result;
    async_op_with_retry(ctx, [&](std::error_code ec, int r) {
        actual_result = std::tuple{ec, r};
        time_taken = std::chrono::steady_clock::now() - start_time;
    });

    ctx.run();
    ASSERT_TRUE(actual_result.has_value());
    EXPECT_EQ(std::get<0>(actual_result.value()), make_error_code(std::errc::io_error));
    EXPECT_EQ(called_times, 5);

    // we expect 4 pauses, 100ms each (first attempt does not have pause)
    EXPECT_NEAR(std::chrono::duration_cast<std::chrono::milliseconds>(time_taken).count(), 400,
                400 / 20);  // 5% tollerance
}

TEST(async_retry_tests, options_are_respected__zero_attempts) {
    asio::io_context ctx;

    auto async_op = [](asio::io_context& ctx, auto done_cb) { done_cb(make_error_code(std::errc::io_error), 0); };

    EXPECT_THROW({ async_retry(ctx, async_op, {.attempts = 0}); }, std::invalid_argument);
}

TEST(async_retry_tests, cancel_test__with_error) {
    asio::io_context ctx;

    // async op returning unique_ptr as result.
    int called_times = 0;
    auto async_op = [&called_times](asio::io_context& ctx, auto done_cb) {
        called_times++;
        done_cb(make_error_code(std::errc::io_error), 0);
    };

    cancellation_token cancel_token;
    auto async_op_with_retry = async_retry(ctx, async_op, {.attempts = 3, .pause = 1s}, cancel_token);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    std::chrono::steady_clock::duration time_taken;

    auto start_time = std::chrono::steady_clock::now();

    async_op_with_retry(ctx, [&](std::error_code ec, int r) {
        actual_result = std::tuple{ec, r};
        time_taken = std::chrono::steady_clock::now() - start_time;
    });

    cancel_token.cancel();

    ctx.run();
    ASSERT_TRUE(actual_result.has_value());
    EXPECT_EQ(std::get<0>(actual_result.value()), make_error_code(asio::error::operation_aborted));
    EXPECT_EQ(called_times, 1);
    EXPECT_NEAR(std::chrono::duration_cast<std::chrono::milliseconds>(time_taken).count(), 0, 5);
}

TEST(async_retry_tests, cancel_on_destruction_test) {
    asio::io_context ctx;

    // async op returning unique_ptr as result.
    int called_times = 0;
    auto async_op = [&called_times](asio::io_context& ctx, auto done_cb) {
        called_times++;
        done_cb(make_error_code(std::errc::io_error), 0);
    };

    auto cancel_token_ptr = std::make_unique<cancellation_token>();
    auto async_op_with_retry = async_retry(ctx, async_op, {.attempts = 3, .pause = 1s}, *cancel_token_ptr);

    std::optional<std::tuple<std::error_code, int>> actual_result;
    std::chrono::steady_clock::duration time_taken;

    auto start_time = std::chrono::steady_clock::now();

    async_op_with_retry(ctx, [&](std::error_code ec, int r) {
        actual_result = std::tuple{ec, r};
        time_taken = std::chrono::steady_clock::now() - start_time;
    });

    cancel_token_ptr = nullptr;

    ctx.run();
    ASSERT_TRUE(actual_result.has_value());
    EXPECT_EQ(std::get<0>(actual_result.value()), make_error_code(asio::error::operation_aborted));
    EXPECT_EQ(called_times, 1);
    EXPECT_NEAR(std::chrono::duration_cast<std::chrono::milliseconds>(time_taken).count(), 0, 5);
}

TEST(async_retry_tests, cancel_test__no_error) {
    asio::io_context ctx;

    // async op returning unique_ptr as result.
    int called_times = 0;
    auto async_op = [&called_times](asio::io_context& ctx, auto done_cb) {
        called_times++;
        done_cb(make_error_code(std::errc::io_error));
    };

    cancellation_token cancel_token;
    auto async_op_with_retry = async_retry(ctx, async_op, {.attempts = 3, .pause = 1s}, cancel_token);

    std::optional<std::error_code> actual_result;
    std::chrono::steady_clock::duration time_taken;

    auto start_time = std::chrono::steady_clock::now();

    async_op_with_retry(ctx, [&](std::error_code ec) {
        actual_result = ec;
        time_taken = std::chrono::steady_clock::now() - start_time;
    });

    cancel_token.cancel();

    ctx.run();
    ASSERT_TRUE(actual_result.has_value());
    EXPECT_EQ(*actual_result, make_error_code(asio::error::operation_aborted));
    EXPECT_EQ(called_times, 1);
    EXPECT_NEAR(std::chrono::duration_cast<std::chrono::milliseconds>(time_taken).count(), 0, 5);
}

// TODO: cancelling operation that has not even been started.

// resuing token shoul work. typical scenario would be either having token as part of class: m_scan_retry_cancel
//  or keeping token inside of some lambda.
