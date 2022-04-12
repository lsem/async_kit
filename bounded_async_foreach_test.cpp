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

auto make_sequence(unsigned n) {
  std::vector<std::string> res;
  for (unsigned i = 0; i < n; ++i)
    res.push_back(std::to_string(i));
  return res;
}

struct test_sample {
  int limit_num;
  int items_num;
  int expected_rinning_num;
};
std::ostream &operator<<(std::ostream &os, const test_sample &s) {
  os << "test_sample(limit_num: " << s.limit_num
     << ", items_num: " << s.items_num
     << ", expected_rinning_num: " << s.expected_rinning_num << ")";
  return os;
}
class bounded_async_foreach_p : public ::testing::TestWithParam<test_sample> {};

TEST_P(bounded_async_foreach_p, all_items_processed_and_limit_respected) {
  test_sample sample = GetParam();

  asio::io_context ctx;

  auto items = make_sequence(sample.items_num);
  std::vector<std::string> items_processed;

  int n_running = 0;
  int n_running_max = 0;

  bounded_async_foreach(
      sample.limit_num, items,
      [&](const std::string &item, auto done_cb) {
        n_running++;
        n_running_max = std::max(n_running_max, n_running);
        items_processed.emplace_back(item);

        async_sleep(ctx, 100ms, [&n_running, done_cb = std::move(done_cb)] {
          n_running--;
          done_cb(std::error_code());
        });
      },
      [&](std::error_code ec) {
        if (sample.limit_num > 0) {
          EXPECT_FALSE(ec);
        }
        EXPECT_EQ(items_processed, items_processed);
      });
  ctx.run();

  EXPECT_EQ(n_running_max, sample.expected_rinning_num);

  if (n_running_max > 0) {
    EXPECT_EQ(items_processed, items);
  }
}

INSTANTIATE_TEST_SUITE_P(
    sequence_size_and_limit_combinations, bounded_async_foreach_p,
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

  bounded_async_foreach(
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
                      done_cb(std::error_code());
                    });
      },
      [&](std::error_code) {
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

    bounded_async_foreach(
        limit_n, items,
        [&](const std::string &item, auto done_cb) {
          n_running++;
          n_running_max = std::max(n_running_max, n_running);
          items_processed.emplace_back(item);

          auto random_time = std::chrono::milliseconds(rand() % 100);
          async_sleep(ctx, random_time,
                      [&n_running, done_cb = std::move(done_cb)] {
                        n_running--;
                        done_cb(std::error_code());
                      });
        },
        [](std::error_code) {
          // ..
        });
    ctx.run();

    EXPECT_EQ(n_running_max, std::min(limit_n, (int)items.size()))
        << "seed was :" << seed;
    EXPECT_EQ(items_processed, items) << "seed was :" << seed;
  }
}

TEST(bounded_async_foreach, empty_collection_test) {
  bool finish_called = false;
  std::vector<int> input;
  bounded_async_foreach(
      1, input,
      [](auto element, auto done) {
        ASSERT_TRUE(false) << "not supposed to be called\n";
      },
      [&](std::error_code ec) {
        EXPECT_FALSE(ec);
        finish_called = true;
      });

  EXPECT_TRUE(finish_called);
}

TEST(bounded_async_foreach, zero_limit_test) {
  bool finish_called = false;
  std::vector<int> input = {1, 2, 3};
  bounded_async_foreach(
      0, input,
      [](auto element, auto done) {
        ASSERT_TRUE(false) << "not supposed to be called\n";
      },
      [&](std::error_code ec) {
        EXPECT_EQ(ec, make_error_code(std::errc::no_child_process));
        finish_called = true;
      });

  EXPECT_TRUE(finish_called);
}

TEST(bounded_async_foreach, synchronous_calls_test) {
  bool finish_called = false;
  std::vector<int> input = {1, 2, 3, 4, 5};
  std::vector<int> done_invocations;
  bounded_async_foreach(
      2, input,
      [&done_invocations](auto element, auto done) {
        done_invocations.emplace_back(element);
        done(std::error_code());
      },
      [&](std::error_code ec) {
        EXPECT_FALSE(ec);
        finish_called = true;
      });

  EXPECT_TRUE(finish_called);
  EXPECT_EQ(done_invocations, input);
}

TEST(bounded_async_foreach, after_cancel_no_new_starts_test) {
  asio::io_context ctx;

  std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  bool finish_called = false;

  bounded_async_foreach(
      3, input,
      [&](int item, auto done_cb) {
        if (item == 1) {
          done_cb(make_error_code(std::errc::operation_canceled));
          return;
        } else {
          ASSERT_TRUE(false);
        }

        async_sleep(ctx, 300ms, [done_cb = std::move(done_cb)] {
          done_cb(std::error_code());
        });
      },
      [&](std::error_code ec) {
        EXPECT_EQ(ec, make_error_code(std::errc::operation_canceled));
        finish_called = true;
      });

  ctx.run();

  EXPECT_TRUE(finish_called);
}

TEST(bounded_async_foreach, finish_called_after_last_item_done) {
  for (int limit = 1; limit < 6; ++limit) {
    asio::io_context ctx;

    std::vector<int> input = {1, 2, 3, 4, 5};
    bool finish_called = false;

    std::vector<int> item_done_calls;

    bounded_async_foreach(
        limit,
        input, // actually should work for both >3 and 1 number of workers.
        [&](int item, auto done_cb) {
          EXPECT_FALSE(finish_called);

          if (item == 3) {
            done_cb(make_error_code(std::errc::operation_canceled));

          } else {
            async_sleep(ctx, 300ms,
                        [item, &item_done_calls, done_cb = std::move(done_cb)] {
                          item_done_calls.push_back(item);
                          done_cb(std::error_code());
                        });
          }
        },
        [&](std::error_code ec) {
          EXPECT_EQ(ec, make_error_code(std::errc::operation_canceled));
          EXPECT_EQ(item_done_calls, std::vector<int>({1, 2}));
          finish_called = true;
        });

    ctx.run();
    EXPECT_TRUE(finish_called);
  }
}

TEST(bounded_async_foreach,
     finish_called_after_last_item_done__pending_jobs_fail) {
  // it is expected that first error is propagated to finish callback,
  // subsequent errors are ignored.
  for (int limit = 3; limit < 6; ++limit) {
    SCOPED_TRACE("limit: " + std::to_string(limit));
    asio::io_context ctx;

    std::vector<int> input = {1, 2, 3, 4, 5};
    bool finish_called = false;

    std::vector<int> item_done_calls;

    bounded_async_foreach(
        limit,
        input, // actually should work for both >3 and 1 number of workers.
        [&](int item, auto done_cb) {
          EXPECT_FALSE(finish_called);

          if (item == 3) {
            done_cb(make_error_code(std::errc::operation_canceled));
          } else {
            async_sleep(ctx, 300ms,
                        [item, &item_done_calls, done_cb = std::move(done_cb)] {
                          item_done_calls.push_back(item);
                          done_cb(make_error_code(std::errc::io_error));
                        });
          }
        },
        [&](std::error_code ec) {
          EXPECT_EQ(ec, make_error_code(std::errc::operation_canceled));
          EXPECT_EQ(item_done_calls, std::vector<int>({1, 2}));
          finish_called = true;
        });

    ctx.run();
    EXPECT_TRUE(finish_called);
  }
}

TEST(bounded_async_foreach, operation_canceled) {

  asio::io_context ctx;

  bool finish_called = false;
  std::vector<int> input = {1, 2, 3, 4, 5};
  std::vector<int> done_invocations;
  bounded_async_foreach(
      2, input,
      [&](auto element, auto done) {
        EXPECT_FALSE(finish_called) << element;
        done_invocations.emplace_back(element);
        auto result = std::error_code();
        auto timeout = std::chrono::milliseconds(10);
        if (element == 1) {
          result = std::make_error_code(std::errc::operation_canceled);
          timeout = std::chrono::milliseconds(5);
        }
        async_sleep(ctx, timeout, [=] { done(result); });
      },
      [&](std::error_code ec) {
        EXPECT_TRUE(ec);
        finish_called = true;
      });

  EXPECT_FALSE(finish_called);
  EXPECT_EQ(done_invocations.size(), 2)
      << "bounded_async_foreach will start 2 iterations immidiatly";

  ctx.run();

  EXPECT_TRUE(finish_called);
  std::vector<int> expected_invocations = {1, 2};
  EXPECT_EQ(done_invocations, expected_invocations);
}

TEST(bounded_async_foreach, first_operation_skips_done_callback) {
  asio::io_context ctx;

  bool finish_called = false;
  std::vector<int> input = {1, 2, 3, 4, 5};
  std::vector<int> done_invocations;
  bounded_async_foreach(
      2, input,
      [&](auto element, auto done) {
        EXPECT_FALSE(finish_called) << element;
        done_invocations.emplace_back(element);
        auto result = std::error_code();
        auto timeout = std::chrono::milliseconds(10);
        if (element == 1) {
          // first iteration skipps done callback
          return;
        }
        async_sleep(ctx, timeout, [=] { done(result); });
      },
      [&](std::error_code ec) {
        EXPECT_TRUE(ec);
        finish_called = true;
      });

  EXPECT_FALSE(finish_called);
  EXPECT_EQ(done_invocations.size(), 2)
      << "bounded_async_foreach will start 2 iterations immidiatly";

  ctx.run();

  // this can be fixed by making bounded_async_foreach wrap/expect finish
  // callback into strong callback.
  EXPECT_FALSE(finish_called);
  std::vector<int> expected_invocations = input;
  EXPECT_EQ(done_invocations, expected_invocations);
}
