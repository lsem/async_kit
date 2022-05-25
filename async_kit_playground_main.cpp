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
#include "ordered_async_ops.hpp"

using namespace std::chrono_literals;

template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::move(cb)](std::error_code ec) { cb(ec); });
}

int main() {
}
