#include "bounded_async_foreach.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <cstdlib>
#include <set>

using namespace std::chrono_literals;

namespace {
template <typename Callback>
void async_sleep(asio::io_context &ctx, std::chrono::steady_clock::duration t,
                 Callback &&cb) {
  auto timer = std::make_shared<asio::steady_timer>(ctx, t);
  timer->async_wait(
      [timer, cb = std::forward<Callback>(cb)](std::error_code ec) { cb(); });
}

} // namespace

auto generate_itmes(unsigned n) {
  std::vector<std::string> res;
  for (unsigned i = 0; i < n; ++i) {
    res.push_back(std::to_string(i));
  }
  return res;
}

struct test_sample {
  int limit_num;
  int items_num;
  int expected_rinning_num;
};

class bounded_async_foreach_test
    : public ::testing::TestWithParam<test_sample> {
protected:
};

TEST_P(bounded_async_foreach_test, all_items_processed) {
  test_sample sample = GetParam();

  asio::io_context ctx;

  auto items = generate_itmes(sample.items_num);
  std::vector<std::string> items_processed;

  int n_running = 0;
  int n_running_max = 0;

  auto foreach_ctx = bounded_async_foreach(
      sample.limit_num, items, [&](const std::string &item, auto done_cb) {
        n_running++;
        n_running_max = std::max(n_running_max, n_running);
        items_processed.emplace_back(item);

        async_sleep(ctx, 100ms, [&n_running, done_cb = std::move(done_cb)] {
          n_running--;
          done_cb();
        });
      });
  ctx.run();

  EXPECT_EQ(n_running_max, sample.expected_rinning_num);

  if (n_running_max > 0)
    EXPECT_EQ(items_processed, items);
}

INSTANTIATE_TEST_SUITE_P(
    bounded_async_foreach_test_tests, bounded_async_foreach_test,
    ::testing::Values(test_sample{14, 100, 14}, test_sample{1, 10, 1},
                      test_sample{0, 100, 0}, test_sample{10, 5, 5}));

TEST(bounded_async_foreach, finished_callback_call_test) {
  // test that 'finished' callback gets called only once and after all 'item'
  // callbacks. finished callback should be called only once.

  asio::io_context ctx;

  auto items = generate_itmes(10);
  std::vector<std::string> items_processed;
  int n_running = 0;
  int n_running_max = 0;
  auto limit_n = 3;

  bool finished_called = false;

  auto foreach_ctx = bounded_async_foreach(
      limit_n, items,
      [&](const std::string &item, auto done_cb) {
        EXPECT_FALSE(finished_called);

        n_running++;
        n_running_max = std::max(n_running_max, n_running);
        items_processed.emplace_back(item);

        auto random_time = std::chrono::milliseconds(rand() % 100);
        async_sleep(ctx, random_time,
                    [&n_running, done_cb = std::move(done_cb)] {
                      n_running--;
                      done_cb();
                    });
      },
      [&]() {
        EXPECT_FALSE(finished_called);
        finished_called = true;
        EXPECT_EQ(items_processed, items_processed);
      });

  ctx.run();
  EXPECT_TRUE(finished_called);
}

TEST(bounded_async_foreach, basic_randomized_test) {
  auto seed = time(NULL);
  srand(seed);

  for (int run_n = 0; run_n < 5; run_n++) {
    asio::io_context ctx;

    int n_running = 0;
    int n_running_max = 0;
    auto limit_n = rand() % 10 + 1;

    auto items = generate_itmes(rand() % 30 + 1);
    std::vector<std::string> items_processed;

    auto foreach_ctx = bounded_async_foreach(
        limit_n, items, [&](const std::string &item, auto done_cb) {
          n_running++;
          n_running_max = std::max(n_running_max, n_running);
          items_processed.emplace_back(item);

          auto random_time = std::chrono::milliseconds(rand() % 100);
          async_sleep(ctx, random_time,
                      [&n_running, done_cb = std::move(done_cb)] {
                        n_running--;
                        done_cb();
                      });
        });
    ctx.run();

    EXPECT_EQ(n_running_max, std::min(limit_n, (int)items.size()))
        << "seed was :" << seed;
    EXPECT_EQ(items_processed, items) << "seed was :" << seed;
  }
}
