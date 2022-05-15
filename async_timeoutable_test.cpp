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

TEST(async_timeoutable_tests, with_results__and_params_test) {
    asio::io_context ctx;

    auto wifi_scan_async = [&ctx](std::string param1, std::string param2, auto scan_done) {
        async_sleep(ctx, 1s, [scan_done, param1, param2] {
            std::vector<std::string> fake_scan_results;
            fake_scan_results.emplace_back(param1);
            fake_scan_results.emplace_back(param2);
            scan_done(std::error_code(), fake_scan_results);
        });
    };

    std::optional<std::error_code> wifi_scan_async_result;
    std::vector<std::string> wifi_scan_async_scan_results;
    wifi_scan_async("1", "2", [&](std::error_code ec, const std::vector<std::string>& scan_results) {
        wifi_scan_async_result = ec;
        wifi_scan_async_scan_results = scan_results;
        ASSERT_FALSE(ec);
        ASSERT_EQ(scan_results.size(), 2);
    });

    // We would like to make it possible to use timeout
    auto wifi_scan_async_with_timeout = async_timeoutable(ctx, wifi_scan_async);

    std::optional<std::error_code> wifi_scan_async_with_timeout_result;
    std::vector<std::string> wifi_scan_async_with_timeout_scan_results;
    wifi_scan_async_with_timeout(10s, "1", "2", [&](std::error_code ec, const std::vector<std::string>& scan_results) {
        wifi_scan_async_with_timeout_result = ec;
        wifi_scan_async_with_timeout_scan_results = scan_results;
        ASSERT_FALSE(ec);
        ASSERT_EQ(scan_results.size(), 2);
    });

    std::optional<std::error_code> wifi_scan_async_with_timeout_result2;
    std::vector<std::string> wifi_scan_async_with_timeout_scan_results2;
    wifi_scan_async_with_timeout(100ms, "1", "2",
                                 [&](std::error_code ec, const std::vector<std::string>& scan_results) {
                                     wifi_scan_async_with_timeout_scan_results2 = scan_results;
                                     wifi_scan_async_with_timeout_result2 = ec;
                                 });

    ctx.run();

    EXPECT_EQ(wifi_scan_async_result, std::error_code());
    EXPECT_EQ(wifi_scan_async_scan_results, std::vector<std::string>({"1", "2"}));

    EXPECT_EQ(wifi_scan_async_with_timeout_result, std::error_code());
    EXPECT_EQ(wifi_scan_async_with_timeout_scan_results, std::vector<std::string>({"1", "2"}));

    EXPECT_EQ(wifi_scan_async_with_timeout_result2, make_error_code(std::errc::timed_out));
    EXPECT_EQ(wifi_scan_async_with_timeout_scan_results2, std::vector<std::string>({}));
}

std::ostream& operator<<(std::ostream& os, std::error_code ec) {
    os << "error message: " << ec.message();
    return os;
}
std::ostream& operator<<(std::ostream& os, std::optional<std::error_code> ec) {
    if (ec.has_value()) {
        os << "error message: " << ec->message();
    } else {
        os << "error message: null";
    }
    return os;
}

TEST(async_timeoutable_tests, without_results__and_params_test) {
    asio::io_context ctx;

    auto wifi_scan_async = [&ctx](std::error_code ec_param, auto scan_done) {
        async_sleep(ctx, 1s, [scan_done, ec_param] { scan_done(ec_param); });
    };

    std::optional<std::error_code> wifi_scan_async_result;
    wifi_scan_async(make_error_code(std::errc::io_error), [&](std::error_code ec) { wifi_scan_async_result = ec; });

    // We would like to make it possible to use timeout
    auto wifi_scan_async_with_timeout = async_timeoutable(ctx, wifi_scan_async);

    std::optional<std::error_code> wifi_scan_async_with_timeout_result;
    wifi_scan_async_with_timeout(10s, make_error_code(std::errc::is_a_directory),
                                 [&](std::error_code ec) { wifi_scan_async_with_timeout_result = ec; });

    std::optional<std::error_code> wifi_scan_async_with_timeout_result2;
    wifi_scan_async_with_timeout(100ms, make_error_code(std::errc::network_reset),
                                 [&](std::error_code ec) { wifi_scan_async_with_timeout_result2 = ec; });

    ctx.run();

    EXPECT_EQ(wifi_scan_async_result, make_error_code(std::errc::io_error));
    EXPECT_EQ(wifi_scan_async_with_timeout_result, make_error_code(std::errc::is_a_directory));
    EXPECT_EQ(wifi_scan_async_with_timeout_result2, make_error_code(std::errc::timed_out));
}
