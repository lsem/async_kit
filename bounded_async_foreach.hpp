#pragma once

#include <functional>
#include <iostream>
#include <memory>

// Apply callback to each element in container but limit number of in items
// being processed.
// Args:
//      Container -- any contianer that has cbegin(), cend()
//      Callback  -- callable object that accepts (const Value& v, Callback
//      done) ).
template <typename Container, typename Callback, typename FinishedCallback>
auto bounded_async_foreach(unsigned n, Container c, Callback cb,
                           FinishedCallback finished) {
  using iter_t = decltype(std::cbegin(c));
  struct algorithm_state {
    iter_t it;
    unsigned n;
    const unsigned N;
    Container c;
    Callback cb;
    FinishedCallback finished_cb;
    algorithm_state(unsigned n, Callback &&cb, FinishedCallback finished_cb)
        : it(), n(n), N(n), cb(std::move(cb)),
          finished_cb(std::move(finished_cb)) {}
    std::function<void()> process_next;
  };

  auto shared_state =
      std::make_shared<algorithm_state>(n, std::move(cb), std::move(finished));

  shared_state->it = std::cbegin(c);
  shared_state->n = n;
  shared_state->c = std::move(c);
  // shared_state->cb = std::move(cb);
  shared_state->process_next =
      [shared_state_weak = std::weak_ptr<algorithm_state>(shared_state),
       finished = std::move(finished)]() {
        if (auto shared_state = shared_state_weak.lock()) {
          if (shared_state->it == std::cend(shared_state->c)) {
            return;
          }
          if (shared_state->n > 0) {
            shared_state->n--;
            shared_state->cb(*shared_state->it++, [shared_state_weak]() {
              if (auto shared_state = shared_state_weak.lock()) {
                // free one vacant place and start next one if there is any
                shared_state->n++;
                const bool finished =
                    shared_state->it == std::cend(shared_state->c);
                if (finished && shared_state->n == shared_state->N) {
                  shared_state->finished_cb();
                } else {
                  shared_state->process_next();
                }
              } else {
                std::cout << "(dbg): cancalled\n";
              }
            });
            // process next if there is vacant place
            shared_state->process_next();
          } else {
            // there are not vacant palces
          }
        } else {
          std::cout << "(dbg): cancalled\n";
        }
      };

  shared_state->process_next();

  auto ctx = [shared_state]() mutable { shared_state.reset(); };
  return ctx;
}

template <typename Container, typename Callback>
auto bounded_async_foreach(unsigned n, Container c, Callback cb) {
  return bounded_async_foreach(n, std::move(c), std::move(cb), []() {});
}
