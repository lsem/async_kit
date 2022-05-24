#pragma once
#include <asio/io_context.hpp>
#include <functional>
#include <vector>

class ordered_async_ops {
   public:
    explicit ordered_async_ops(asio::io_context& ctx) : m_ctx(ctx) {}

    template <class AsyncHandler>
    auto operator()(int priority, AsyncHandler h) {
        m_ops_total_n++;
        return [this, priority, h = std::move(h)](std::error_code ec) {
            m_thunks.emplace_back(priority, [this, ec, h = std::move(h)]() { h(ec); });
            if (m_thunks.size() == m_ops_total_n) {
                // done, sort and call thunks
                std::sort(m_thunks.begin(), m_thunks.end(),
                          [](auto& l, auto& r) { return std::get<int>(l) < std::get<int>(r); });
                for (auto& t : m_thunks) {
                    m_ctx.post([t = std::move(t)] { std::get<std::function<void()>>(t)(); });
                    // for simplicity, left in undefined state.
                }
            }
        };
    }

    asio::io_context& m_ctx;
    std::vector<std::tuple<int, std::function<void()>>> m_thunks;
    int m_ops_total_n = 0;
};
