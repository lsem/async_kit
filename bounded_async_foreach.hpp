#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <system_error>

// Apply callback to each element in container but limit number of in items
// being processed.
// Args:
//      Container -- any contianer that has cbegin(), cend()
//      Callback  -- callable object that accepts (const Value& v, Callback
//      done) ).
template <typename Container, typename Callback, typename FinishCallback>
void bounded_async_foreach(unsigned n, Container c, Callback cb,
                           FinishCallback finished_cb) {
  if (n == 0) {
    finished_cb(make_error_code(std::errc::no_child_process));
    return;
  } else if (std::empty(c)) {
    // nothing has beem processed succesfully.
    finished_cb(std::error_code());
    return;
  }

  using iter_type = decltype(std::cbegin(c));
  struct algorithm_state {
    Container c;
    iter_type it;
    Callback cb;
    unsigned n;
    const unsigned N;
    FinishCallback finished_cb;
    bool finish_called = false;
    std::error_code exec_result = std::error_code();

    algorithm_state(Container c, Callback cb, unsigned n,
                    FinishCallback finished_cb)
        : c(std::move(c)), it(std::cbegin(this->c)), cb(std::move(cb)), n(n),
          N(n), finished_cb(std::move(finished_cb)) {}
    std::function<void()> process_next;
  };

  auto shared_state = std::make_shared<algorithm_state>(
      std::move(c), std::move(cb), n, std::move(finished_cb));

  shared_state->process_next = [shared_state]() {
    if (shared_state->it == std::cend(shared_state->c)) {
      if (shared_state->n == shared_state->N) {
        // last element tries to find new work
        shared_state->finished_cb(shared_state->exec_result);
      } else {
        // just do not start new anymore.
      }
    } else {
      // more to do
      if (shared_state->n > 0) {
        // there are more place to start new work
        shared_state->n--;

        shared_state->cb(*shared_state->it++,
                         [shared_state](std::error_code ec) {
                           shared_state->n++;
                           if (ec) {
                             // we stop at error, but want to finish after the
                             // last element finished.
                             shared_state->exec_result = ec;
                             shared_state->it = std::cend(shared_state->c);
                           } else {
                             // success
                           }

                           shared_state->process_next();
                         });

        shared_state->process_next();
      } else {
        // the ones currently working will continue.
      }
    }
  };

  shared_state->process_next();
}
