#include "call_monitor.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <system_error>
#include <thread>

namespace call_monitor {

namespace {
class call_monitor_impl {
   public:
    ~call_monitor_impl() { stop(); }

    void run(std::function<void(std::string)> f) {
        m_log_write = std::move(f);
        m_th = std::thread([this]() {
            asio::io_context::work w(m_ctx);
            m_ctx.run();
        });
    }

    void stop() {
        if (m_th.joinable()) {
            m_ctx.stop();
            m_th.join();
        }
    }

    std::shared_ptr<void> call_guard(std::string id, std::chrono::steady_clock::duration d) {
        auto timer = std::make_shared<asio::steady_timer>(m_ctx);

        if (!m_th.joinable()) {
            // allow make calls on not started monitors, for unit tests.
            return nullptr;
        }

        timer->expires_after(d);
        timer->async_wait([id, d, this](std::error_code ec) {
            if (!ec) {
                if (m_log_write) {
                    std::stringstream ss;
                    const auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
                    ss << "CALL HANG: '" << id << "' >" << time_ms << "ms\n";
                    m_log_write(ss.str());
                }
            }
        });

        return timer;
    }

   private:
    std::thread m_th;
    asio::io_context m_ctx;
    std::function<void(std::string)> m_log_write = nullptr;
};

call_monitor_impl the_instance;
}  // namespace

void start(std::function<void(std::string)> f) {
    the_instance.run(std::move(f));
}

void stop() {
    the_instance.stop();
}

void report_hang(std::function<void()> callable, std::string call_id, std::chrono::steady_clock::duration t) {
    auto guard = the_instance.call_guard(std::move(call_id), t);
    callable();
}

}  // namespace call_monitor
