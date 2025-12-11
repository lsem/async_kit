#pragma once

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <vector>

namespace async_kit {

class async_critical_section : public std::enable_shared_from_this<async_critical_section> {
   public:
    explicit async_critical_section(asio::io_context& ctx) : m_ctx(ctx) {}

    ~async_critical_section() {
        // we still need to call callbacks.
        if (!m_waiting_list.empty()) {
            for (auto& cb : m_waiting_list) {
		asio::post(m_ctx, [cb = std::move(cb)] {
                    // in case of error we give unfunctional/empty done callback.
                    cb(make_error_code(std::errc::operation_canceled), []() {});
                });
            }
        }
    }

    // TODO: std::function does not support move-only callbacks.
    using next_callback_t = std::function<void(std::error_code, std::function<void()>)>;

   public:
    template <class Callable>
    void async_enter(Callable cb) {
        assert(shared_from_this() && "created not as shared pointer");
        if (m_busy) {
            m_waiting_list.push_back(std::move(cb));
            // std::cout << "list size: " << m_waiting_list.size() << "\n";
        } else {
            m_busy = true;
            // got its lock.
            do_run(std::move(cb));
        }
    }

    void do_run(next_callback_t cb) {
        cb(std::error_code(), [weak_this = weak_from_this()]() {
            if (auto this_ = weak_this.lock()) {
		asio::post(this_->m_ctx, [weak_this = this_->weak_from_this()] {
                    if (auto this_ = weak_this.lock()) {
                        this_->run_next();
                    }
                });
            }
        });
    }

    void run_next() {
        if (!m_waiting_list.empty()) {
            auto next = std::move(m_waiting_list.front());
            m_waiting_list.pop_front();
            // next got lock and will be executed.
	    asio::post(m_ctx, [next = std::move(next), weak_this = weak_from_this()] {
                if (auto this_ = weak_this.lock()) {
                    this_->do_run(std::move(next));
                }
            });
        } else {
            // std::cout << "became free\n";
            m_busy = false;
        }
    }

    asio::io_context& m_ctx;
    bool m_busy = false;
    std::list<next_callback_t> m_waiting_list;
};

std::shared_ptr<async_critical_section> make_async_critical_section(asio::io_context& ctx) {
    return std::make_shared<async_critical_section>(ctx);
}

}  // namespace async_kit
