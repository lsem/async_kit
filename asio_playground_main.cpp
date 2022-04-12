#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include "bounded_async_foreach.hpp"

using namespace std::chrono_literals;

template <typename Callback>
void async_sleep(asio::io_context &ctx, std::chrono::steady_clock::duration t,
                 Callback &&cb) {
  auto timer = std::make_shared<asio::steady_timer>(ctx, t);
  timer->async_wait(
      [timer, cb = std::forward<Callback>(cb)](std::error_code ec) { cb(); });
}

int num_in_flight = 0;

template <class Callback>
void start_work(asio::io_context &ctx, Callback done) {

  auto generate_itmes = [](unsigned n) {
    std::vector<std::string> res;
    while (n--) {
      res.emplace_back(std::to_string(n));
    }
    return res;
  };

  std::vector<int> items = { 1,2,3 };
  struct state_t {
      std::stringstream log;
      bool done_called = false;
  };
  auto self = std::make_shared<state_t>();

  bounded_async_foreach(
      2, items,
      [&ctx, self](int i, auto cb) {

        if (self->done_called) {
            self->log << "done was already called\n";
        }
        self->log << "processing item [" << i << "] started\n";

        int timeout = i == 1 ? 100 : 200;

        async_sleep(ctx, std::chrono::milliseconds(timeout),
            [self, i, cb = std::move(cb)]{
              self->log << "processing item [" << i << "] done\n";
            if (i == 1) {
                self->log << "Operation failed\n";
                cb(std::make_error_code(std::errc::operation_canceled));
                return;
            }
            cb({});
            });
        if (self->done_called) {
            self->log << "done was already called\n";
        }
      },
      [self, done = std::move(done)](std::error_code ec) {
          self->done_called = true;
        self->log << "finished with " << ec.message().c_str() << "\n";
        done(self->log.str());
      });
}

int main() {
  std::cout << "asio playground\n";

  asio::io_context ctx;

  static auto expected_log = R"(asio playground
processing item [1] started
processing item [2] started
processing item [1] done
Operation failed
processing item [2] done
finished with operation canceled
)";

  start_work(ctx, [](auto log) { 
      
      if (log != expected_log) {
          std::cout << "Wrong logs\n\n"
              << " -- Expected logs:\n\n"
              << expected_log
              << "\n -- Actual logs\n\n"
              << log;
      }
      });

  // // simulate cancel after 3s
  // async_sleep(ctx, 2s, [&foreach_ctx]() {
  //   std::cout << "time out, cancelling async foreach...\n";
  //   foreach_ctx();
  // });

  ctx.run();
}