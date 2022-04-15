#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <iostream>
#include <system_error>
#include <thread>

// TODO: move to cpp
namespace details {
class sync_call_monitor {
public:
  ~sync_call_monitor() { stop(); }

  void run() {
    m_th = std::thread([this]() {
      asio::io_context::work w(m_ctx);
      m_ctx.run();
    });
  }

  void stop() {
    m_ctx.stop();
    if (m_th.joinable()) {
      m_th.join();
    }
  }

  std::shared_ptr<void> call_guard(std::string id,
                                   std::chrono::steady_clock::duration d) {
    auto timer = std::make_shared<asio::steady_timer>(m_ctx);

    if (!m_th.joinable()) {
      // allows to not run this supplementory service for unit tests.
      return nullptr;
    }

    timer->expires_after(d);
    timer->async_wait([id, d](std::error_code ec) {
      if (!ec) {
        // TODO: actually we need to lock some mutex here for accessing logger.
        std::cerr
            << "HANG: '" << id << "' >"
            << std::chrono::duration_cast<std::chrono::milliseconds>(d).count()
            << "ms\n";
      }
    });
    return timer;
  }

private:
  std::thread m_th;
  asio::io_context m_ctx;
};

sync_call_monitor m_instrance;

} // namespace details

namespace call_monitor {
void start() { details::m_instrance.run(); }
void stop() { details::m_instrance.stop(); }

template <class Callable>
void report_hang(Callable &&cb, std::string call_id,
                 std::chrono::steady_clock::duration t) {
  auto guard = details::m_instrance.call_guard(std::move(call_id), t);
  cb();
}

} // namespace call_monitor
