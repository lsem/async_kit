#pragma once

#include <asio/io_context.hpp>
#include <functional>
#include <list>
#include <memory>
#include <vector>

namespace async_kit {

class async_critical_section : public std::enable_shared_from_this<async_critical_section> {
   public:
    explicit async_critical_section(asio::io_context& ctx) : m_ctx(ctx) {}

   private:
    using next_callback_t = std::function<void(std::error_code, std::function<void()>)>;

   public:
    template <class Callable>
    void async_enter(Callable cb) {
        assert(shared_from_this() && "created not as shared pointer");
        if (m_busy) {
            m_waiting_list.push_back(std::move(cb));
        } else {
            m_busy = true;
            // got its lock.
            do_run(std::move(cb));
        }
    }

    void do_run(next_callback_t cb) {
        cb(std::error_code(), [weak_this = weak_from_this()]() {
            // lock released, we need to run next somehow, the thing is that we
            // need shared pointer now.
            if (auto this_ = weak_this.lock()) {
                this_->m_ctx.post([weak_this = this_->weak_from_this()] {
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
            m_ctx.post([next = std::move(next), weak_this = weak_from_this()] {
                if (auto this_ = weak_this.lock()) {
                    this_->do_run(std::move(next));
                }
            });
        } else {
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
