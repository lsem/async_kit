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

using namespace lsem::async;

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

TEST(async_timeoutable_tests, wrapped_callable_can_passed_as_rvalue) {
    asio::io_context ctx;

    auto wifi_scan_async = [&ctx, u = std::make_unique<int>(42)](auto scan_done) {
        async_sleep(ctx, 1s, [scan_done] { scan_done(make_error_code(std::errc::io_error)); });
    };

    auto wifi_scan_async_with_timeout = async_timeoutable(ctx, std::move(wifi_scan_async));
    std::optional<std::error_code> wifi_scan_async_with_timeout_result;
    wifi_scan_async_with_timeout(10s, [&](std::error_code ec) { wifi_scan_async_with_timeout_result = ec; });

    ctx.run();

    EXPECT_EQ(wifi_scan_async_with_timeout_result, make_error_code(std::errc::io_error));
}

TEST(async_timeoutable_tests, wrapped_callable_can_passed_as_lvalue) {
    asio::io_context ctx;

    // use std function here which will call bad_function_call in case it was moved (I can't quickly find better way to
    // check it).
    std::function<void(std::function<void(std::error_code)>)> wifi_scan_async = [&ctx](auto scan_done) {
        async_sleep(ctx, 1s, [scan_done] { scan_done(std::error_code()); });
    };

    auto wifi_scan_async_with_timeout = async_timeoutable(ctx, wifi_scan_async);
    wifi_scan_async_with_timeout(10s, [](std::error_code ec) {});

    // we expect that original wifi_scan_async has not been moved but copied so original one
    // is callable and functioning correctly.
    ASSERT_TRUE(wifi_scan_async != nullptr) << "decorated function was moved";
    std::optional<std::error_code> wifi_scan_async_result;
    wifi_scan_async([&](std::error_code ec) { wifi_scan_async_result = ec; });

    ctx.run();

    EXPECT_EQ(wifi_scan_async_result, std::error_code());
}

TEST(async_timeoutable_tests, args_forwarding_tests) {
    // this test verifies that passed arguments are properly forwarded, not copied or moved.
    asio::io_context ctx;

    //
    // test 1
    //
    auto async_func_with_int_ref_arg = [&ctx](int& int_ref, auto scan_done) {
        async_sleep(ctx, 0ms, [scan_done, &int_ref] {
            int_ref = 42;
            scan_done(std::error_code());
        });
    };
    auto async_func_with_int_ref_arg_with_timeout = async_timeoutable(ctx, async_func_with_int_ref_arg);
    int answer = 0;
    async_func_with_int_ref_arg(answer, [&](std::error_code ec) {});
    int answer2 = 0;
    async_func_with_int_ref_arg_with_timeout(10s, answer2, [&](std::error_code ec) {});

    // // pass rvalue (should not compile)
    // async_func_with_int_ref_arg_with_timeout(10s, 101, [&](std::error_code ec) {});

    //
    // test 2
    //
    auto async_func_with_unique_ptr_arg = [&ctx](std::unique_ptr<int> p, auto done) {
        done(std::error_code(), std::move(p));
    };
    int answer3 = 0;
    async_func_with_unique_ptr_arg(std::make_unique<int>(42), [&answer3](std::error_code ec, std::unique_ptr<int> p) {
        ASSERT_TRUE(p);
        answer3 = *p;
    });

    int answer4 = 0;
    auto async_func_with_unique_ptr_arg_with_timout = async_timeoutable(ctx, async_func_with_unique_ptr_arg);
    async_func_with_unique_ptr_arg_with_timout(10s, std::make_unique<int>(42),
                                               [&answer4](std::error_code ec, std::unique_ptr<int> p) {
                                                   ASSERT_TRUE(p);
                                                   answer4 = *p;
                                               });
    ctx.run();

    EXPECT_EQ(answer, 42);
    EXPECT_EQ(answer2, answer);
    EXPECT_EQ(answer3, 42);
    EXPECT_EQ(answer4, answer3);
}

TEST(async_timeoutable_tests, done_noncopyable_test) {
    asio::io_context ctx;

    auto simple_async_func = [](auto done) { done(std::error_code()); };

    // test original function with callback that is move-only.
    simple_async_func([x = std::make_unique<int>(42)](std::error_code) {});

    // make it timeoutable
    auto simple_async_func_with_timout = async_timeoutable(ctx, simple_async_func);
    // and test that move only callback still supported.
    simple_async_func_with_timout(1s, [x = std::make_unique<int>(42)](std::error_code) {});

    ctx.run();
}

TEST(async_timeoutable_tests, callable_can_be_std_function_like_test) {
    asio::io_context ctx;

    std::function<void(std::function<void(std::error_code)>)> simple_async_stdfunc = [](auto done) {
        done(make_error_code(std::errc::io_error));
    };

    // make it timeoutable (pass as lvalue)
    auto simple_async_func_with_timout = async_timeoutable(ctx, simple_async_stdfunc);

    std::optional<std::error_code> result;
    simple_async_func_with_timout(1s, [&result, x = std::make_unique<int>(42)](std::error_code ec) { result = ec; });

    ctx.run();

    ASSERT_EQ(result, make_error_code(std::errc::io_error));
}

// we specifically don't use template here because to use template we need use so called LIFT macro.
void simple_free_function(std::function<void(std::error_code)> done) {
    done(make_error_code(std::errc::io_error));
};

TEST(async_timeoutable_tests, callable_can_be_free_function_teset) {
    asio::io_context ctx;

    // make it timeoutable (pass as lvalue)
    auto simple_free_function_with_timeout = async_timeoutable(ctx, simple_free_function);

    std::optional<std::error_code> result;
    simple_free_function_with_timeout(1s,
                                      [&result, x = std::make_unique<int>(42)](std::error_code ec) { result = ec; });

    ctx.run();

    ASSERT_EQ(result, make_error_code(std::errc::io_error));
}

// we specifically don't use template here because to use template we need use so called LIFT macro.
template <class Callable>
void simple_free_function_template(Callable&& done) {
    done(make_error_code(std::errc::io_error));
};

TEST(async_timeoutable_tests, callable_can_be_template_if_lifting_employed) {
    asio::io_context ctx;

    // make it timeoutable (lifting needed because we can't get address of template function)
    auto simple_free_function_template_with_timeout = async_timeoutable(
        ctx, [](auto&&... args) { simple_free_function_template(std::forward<decltype(args)>(args)...); });

    std::optional<std::error_code> result;
    simple_free_function_template_with_timeout(
        1s, [&result, x = std::make_unique<int>(42)](std::error_code ec) { result = ec; });

    ctx.run();

    ASSERT_EQ(result, make_error_code(std::errc::io_error));
}

TEST(async_timeoutable_tests, done_callback_destroyed_when_timeout) {
    asio::io_context ctx;

    // user may want to control lifetime of source  to results callback to control resources so
    // we need to make sure that in case timeout occured, original operation if not cancelled
    // should not prolong life of callback.

    auto simple_async_function = [&ctx](auto done) {
        async_sleep(ctx, 1s, [&ctx, done = std::move(done)] { done(std::error_code()); });
    };

    auto simple_async_function_with_timeout = async_timeoutable(ctx, simple_async_function);

    auto resource_ptr = std::make_shared<int>(1);

    simple_async_function_with_timeout(100ms, [&ctx, resource_ptr](std::error_code ec) {
        ASSERT_EQ(ec, make_error_code(std::errc::timed_out));
        // it is expected that only this ref and in outer scope exist.
        ctx.post([resource_ptr] { ASSERT_EQ(resource_ptr.use_count(), 2); });
    });

    ASSERT_EQ(resource_ptr.use_count(), 2);

    ctx.run();

    ASSERT_EQ(resource_ptr.use_count(), 1);
}

TEST(async_timeoutable_tests, done_callback_destroyed_when_results) {
    asio::io_context ctx;

    // user may want to control lifetime of source  to results callback to control resources so
    // we need to make sure that in case timeout occured, original operation if not cancelled
    // should not prolong life of callback.

    auto simple_async_function = [&ctx](auto done) {
        async_sleep(ctx, 100ms, [&ctx, done = std::move(done)] { done(std::error_code()); });
    };

    auto simple_async_function_with_timeout = async_timeoutable(ctx, simple_async_function);

    auto resource_ptr = std::make_shared<int>(1);

    simple_async_function_with_timeout(1s, [&ctx, resource_ptr](std::error_code ec) {
        ASSERT_EQ(ec, std::error_code());
        // it is expected that only this ref and in outer scope exist.
        ctx.post([resource_ptr] { ASSERT_EQ(resource_ptr.use_count(), 2); });
    });

    ASSERT_EQ(resource_ptr.use_count(), 2);

    ctx.run();

    ASSERT_EQ(resource_ptr.use_count(), 1);
}

struct simple_socket {
    explicit simple_socket(asio::io_context& ctx, std::chrono::steady_clock::duration simulated_shutdown_time)
        : m_simulated_shutdown_time(simulated_shutdown_time), m_timer(ctx) {}

    template <class Handler>
    void async_shutdown(Handler&& h) {
        // to simulate async long running process, we are going to have timer.
        m_timer.expires_from_now(m_simulated_shutdown_time);
        m_timer.async_wait([h = std::move(h)](std::error_code ec) { h(std::error_code()); });
    }

    void close() { m_timer.cancel(); }

    std::chrono::steady_clock::duration m_simulated_shutdown_time;
    asio::steady_timer m_timer;
};

TEST(async_timeoutable_tests, use_case_with_socket) {
    asio::io_context ctx;

    // imagine we have socket which has async operation shutdown.
    simple_socket sock(ctx, 5s);

    // we want to get shutdown functionality accept timeout

    // lets turn member .async_shutdown to free function like object accepting socket as param.
    // but we want to make sure that original function is temporary, even though referenced as lvalue.
    std::function<void(std::chrono::steady_clock::duration, simple_socket&, std::function<void(std::error_code)>)>
        shutdown_func_with_tm;
    {
        auto shutdown_func = [](simple_socket& sock, std::function<void(std::error_code)> done) {
            sock.async_shutdown(std::move(done));
        };
        // passing as lvalue but we don't generally care as far as we don't have resources in
        // shutdown_func (lambda capture list is empty).
        shutdown_func_with_tm = async_timeoutable(ctx, shutdown_func);
    }

    std::optional<std::error_code> result;
    shutdown_func_with_tm(10ms, sock, [&result, &sock](std::error_code ec) {
        ASSERT_FALSE(result.has_value());
        result = make_error_code(std::errc::timed_out);
        sock.close();
    });

    ctx.run();

    ASSERT_EQ(result, make_error_code(std::errc::timed_out));
}

TEST(async_timeoutable_tests, use_case_with_socket_2) {
    asio::io_context ctx;

    simple_socket sock(ctx, 5s);

    std::optional<std::error_code> result;
    {
        // similar to use_case_with_socket but this time we generate member-to-free-func adaptor on the fly,
        // as rvalue. furthermore we remove it immidiately after running it.
        auto shutdown_func_with_tm =
            async_timeoutable(ctx, [](simple_socket& sock, auto done) { sock.async_shutdown(std::move(done)); });

        shutdown_func_with_tm(10ms, sock, [&result, &sock](std::error_code ec) {
            ASSERT_FALSE(result.has_value());
            result = make_error_code(std::errc::timed_out);
            sock.close();
        });
    }

    ctx.run();

    ASSERT_EQ(result, std::errc::timed_out);
}
