#include "call_monitor.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include "bounded_async_foreach.hpp"

using namespace std::chrono_literals;

template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::forward<Callback>(cb)](std::error_code ec) { cb(); });
}

int main() {
    std::cout << "asio playground\n";
    asio::io_context ctx;

    // warning: beware of thread safety here.
    call_monitor::start([](std::string s) { std::cout << s; });

    ctx.post([&] {
        call_monitor::sync_call(
            [&] {
                std::cout << "sleep(5s).begin\n";
                std::this_thread::sleep_for(5s);
                std::cout << "sleep(5s).end\n";
            },
            "sleep 5s", 1s);

        std::cout << "sleep more\n";
        std::this_thread::sleep_for(3s);
        std::cout << "end\n";
    });

    ctx.run();
}