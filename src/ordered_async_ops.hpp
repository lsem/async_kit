#pragma once
#include <asio/io_context.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>

namespace details {
class ordered_async_ops_impl : public std::enable_shared_from_this<ordered_async_ops_impl> {
   public:
    explicit ordered_async_ops_impl(asio::io_context& ctx) : m_ctx(ctx) {}

    template <class AsyncHandler>
    auto operator()(int order, AsyncHandler h) {
        m_expected_order.emplace_back(order);
        m_ops_total_n++;
        auto this_ = shared_from_this();
        return [this_, order, h = std::move(h)](std::error_code ec) {
            this_->m_thunks.emplace_back(order, [ec, h = std::move(h)]() { h(ec); });
            if (this_->can_resovle()) {
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

    bool can_resovle() const {
        auto execpted_order = m_expected_order;
        std::sort(execpted_order.begin(), execpted_order.end());

        std::vector<int> actual_order;
        for (auto& [order, _] : m_thunks) {
            actual_order.push_back(order);
        }
        std::sort(actual_order.begin(), actual_order.end());
        // we are not waiting last element if there  are multiple ones.
        if (execpted_order.size() > 1) {
            execpted_order.resize(execpted_order.size() - 1);
        }
        std::cout << "expected:\n";
        for (auto x : execpted_order) {
            std::cout << x << "\n";
        }
        std::cout << "actual:\n";
        for (auto x : actual_order) {
            std::cout << x << "\n";
        }
        return execpted_order == actual_order;
    }

    // to find out how when we are ready we need to wait for first, then for second, then for third,
    // we can not wait for last one and immidiately resolve callbacks allowing trailing callbacks to go through
    // component as is. to find out whether criteria is met we ned to compare what was

    asio::io_context& m_ctx;
    std::vector<std::tuple<int, std::function<void()>>> m_thunks;
    std::vector<int> m_expected_order;
    int m_ops_total_n = 0;
};
}  // namespace details

class ordered_async_ops {
   public:
    explicit ordered_async_ops(asio::io_context& ctx)
        : m_impl(std::make_shared<details::ordered_async_ops_impl>(ctx)) {}

    template <class AsyncHandler>
    auto operator()(int order, AsyncHandler&& h) {
        return m_impl->operator()(order, std::forward<AsyncHandler>(h));
    }

   private:
    std::shared_ptr<details::ordered_async_ops_impl> m_impl;
};
