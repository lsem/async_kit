#pragma once
#include <asio/io_context.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace details {
class ordered_async_ops_impl : public std::enable_shared_from_this<ordered_async_ops_impl> {
   public:
    explicit ordered_async_ops_impl(asio::io_context& ctx) : m_ctx(ctx) {}

    template <class AsyncHandler>
    auto operator()(int priority, AsyncHandler h) {
        m_ops_total_n++;
        auto this_ = shared_from_this();
        return [this_, priority, h = std::move(h)](std::error_code ec) {
            this_->m_thunks.emplace_back(priority, [ec, h = std::move(h)]() { h(ec); });
            if (this_->m_thunks.size() == this_->m_ops_total_n) {
                // done, sort and call thunks
                std::sort(this_->m_thunks.begin(), this_->m_thunks.end(),
                          [](auto& l, auto& r) { return std::get<int>(l) < std::get<int>(r); });
                for (auto& t : this_->m_thunks) {
                    this_->m_ctx.post([t = std::move(t)] { std::get<std::function<void()>>(t)(); });
                    // for simplicity, left in undefined state.
                }
            }
        };
    }

    asio::io_context& m_ctx;
    std::vector<std::tuple<int, std::function<void()>>> m_thunks;
    int m_ops_total_n = 0;
};
}  // namespace details

class ordered_async_ops {
   public:
    explicit ordered_async_ops(asio::io_context& ctx)
        : m_impl(std::make_shared<details::ordered_async_ops_impl>(ctx)) {}

    template <class AsyncHandler>
    auto operator()(int priority, AsyncHandler&& h) {
        return m_impl->operator()(priority, std::forward<AsyncHandler>(h));
    }

   private:
    std::shared_ptr<details::ordered_async_ops_impl> m_impl;
};
