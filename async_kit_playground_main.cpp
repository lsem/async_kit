#include "call_monitor.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "bounded_async_foreach.hpp"

using namespace std::chrono_literals;

template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::move(cb)](std::error_code ec) { cb(ec); });
}

int main() {
    asio::io_context ctx;

    // thunks with priorities
    std::vector<std::tuple<std::function<void()>, int>> thunks;

    auto ready = [&thunks, &ctx]() {
        std::sort(thunks.begin(), thunks.end(),
                  [](auto& left, auto& right) { return std::get<int>(left) < std::get<int>(right); });

        // call deferred handelrs when they are now in correct order
        for (auto& thunk : thunks) {
            ctx.post([thunk = std::move(thunk)] { std::get<std::function<void()>>(thunk)(); });
        }
    };

    // this is logically op1
    async_sleep(ctx, 1s, [&thunks, ready](std::error_code ec) {
        std::cout << "DEBUG: RECEIVE done\n";
        auto thunk = [ec]() {
            // handling happens here.
            // ..
            std::cout << "RECEIVE handler working, error code: " << ec.message() << "\n";
        };
        thunks.emplace_back(std::move(thunk), 1);  // 1 priority is lower than 1
        if (thunks.size() == 2) {
            ready();
        }
    });

    // and this is logically op2
    async_sleep(ctx, 2s, [&thunks, ready](std::error_code ec) {
        std::cout << "DEBUG: SEND done\n";
        auto thunk = [ec]() {
            // handling happens here.
            // ..
            std::cout << "SEND handler working, error code: " << ec.message() << "\n";
        };
        thunks.emplace_back(std::move(thunk), 0);  // 0 priority is higher than 1
        if (thunks.size() == 2) {
            ready();
        }
    });

    ctx.run();
}
