#include "async_criticial_section.hpp"

#include <gtest/gtest.h>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <chrono>

using namespace std::chrono_literals;

namespace {
template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::move(cb)](std::error_code ec) { cb(); });
}
}  // namespace

TEST(async_critical_section_tests, basic_test) {
    asio::io_context ctx;

    std::vector<int> events;

    auto cs = async_kit::make_async_critical_section(ctx);

    cs->async_enter([&](std::error_code ec, auto done_cb) {
        ASSERT_FALSE(ec);
        events.emplace_back(10);
        async_sleep(ctx, 30ms, [&, done_cb = std::move(done_cb)] {
            events.emplace_back(11);
            done_cb();
        });
    });

    cs->async_enter([&](std::error_code ec, auto done_cb) {
        ASSERT_FALSE(ec);
        events.emplace_back(20);
        async_sleep(ctx, 30ms, [&, done_cb = std::move(done_cb)] {
            events.emplace_back(21);
            done_cb();
        });
    });

    cs->async_enter([&](std::error_code ec, auto done_cb) {
        ASSERT_FALSE(ec);
        events.emplace_back(30);
        async_sleep(ctx, 30ms, [&, done_cb = std::move(done_cb)] {
            events.emplace_back(31);
            done_cb();
        });
    });

    ctx.run();

    ASSERT_EQ(events, std::vector<int>({10, 11, 20, 21, 30, 31}));
}

TEST(async_critical_section_tests, randomized_tests) {
    asio::io_context ctx;

    std::vector<int> events;

    auto cs = async_kit::make_async_critical_section(ctx);

    for (int i = 0; i < 100; ++i) {
        // make random pause and then try enter
        auto entry_pause = std::chrono::milliseconds(rand() % 1000 * 7);
        async_sleep(ctx, entry_pause, [&ctx, &events, cs, i] {
            cs->async_enter([&ctx, i, cs, &events](std::error_code ec, auto done_cb) {
                ASSERT_FALSE(ec);
                events.emplace_back(i * 10);
                auto work_time = std::chrono::milliseconds(rand() % 10 * 10);
                async_sleep(ctx, work_time, [&events, i, done_cb = std::move(done_cb)] {
                    events.emplace_back(i * 10 + 1);
                    done_cb();
                });
            });
        });
    }

    ctx.run();

    for (size_t i = 0; i < events.size() - 1; i += 2) {
        ASSERT_EQ(events[i + 1], events[i] + 1);
    }

    ASSERT_EQ(events.size(), 200);
    std::sort(events.begin(), events.end(), [](auto x, auto y) { return x < y; });
    for (int i = 0, j = 0; i < 200; i += 2, j++) {
        ASSERT_EQ(events[i], j * 10);
        ASSERT_EQ(events[i + 1], j * 10 + 1);
    }
}
