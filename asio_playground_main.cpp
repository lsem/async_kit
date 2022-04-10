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
      [timer, cb = std::forward<Callback>(cb)](std::error_code ec) { cb(); });
}

int num_in_flight = 0;

auto start_work(asio::io_context& ctx) {


    auto generate_itmes = [](unsigned n) {
        std::vector<std::string> res;
        while (n--) {
            res.emplace_back(std::to_string(n));
        }
        return res;
    };

    auto items = generate_itmes(300);

    auto foreach_ctx = bounded_async_foreach(
        14, items, [&ctx](auto& item, auto done_cb) {
            std::cout << "processing item [" << item << "] started "
                << "  (num in flight: " << ++num_in_flight << ")\n";

            // simulate long running operation and signal finish by calling done_cb
            async_sleep(ctx, std::chrono::milliseconds((rand() % 5) * 100),
                [item, done_cb = std::move(done_cb)]{
                  std::cout << "processing item [" << item << "] done "
                            << "     (num in flight: " << --num_in_flight
                            << ")\n";
                  done_cb();
                });
        });
    return foreach_ctx;
}

int main() {
  std::cout << "asio playground\n";

  asio::io_context ctx;


  auto foreach_ctx = start_work(ctx);

  // simulate cancel after 3s
  async_sleep(ctx, 2s, [&foreach_ctx]() {
    std::cout << "time out, cancelling async foreach...\n";
    foreach_ctx();
  });

  ctx.run();
}