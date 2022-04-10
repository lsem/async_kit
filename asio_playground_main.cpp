#include <chrono>
#include <iostream>
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
      [timer, cb = std::forward<Callback>(cb)](std::error_code ec) {
        if (ec != asio::error::operation_aborted) {
          cb();
        }
      });
}

int main() {
  std::cout << "asio playground\n";

  asio::io_context ctx;

  int num_in_flight = 0;

  auto generate_itmes = [](unsigned n) {
    std::vector<std::string> res;
    while (n--) {
      res.emplace_back(std::to_string(n));
    }
    return res;
  };

  bounded_async_foreach(14, generate_itmes(100), [&](auto &item, auto done_cb) {
    std::cout << "processing item [" << item << "] started "
              << "  (num in flight: " << ++num_in_flight << ")\n";

    // simulate long running operation
    async_sleep(ctx, std::chrono::seconds(rand() % 4 + 1),
                [&num_in_flight, item, done_cb = std::move(done_cb)] {
                  std::cout << "processing item [" << item << "] done "
                            << "     (num in flight: " << --num_in_flight << ")\n";
                  done_cb();
                });
  });
  ctx.run();
}