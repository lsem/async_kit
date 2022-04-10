#pragma once

#include <functional>
#include <memory>
#include <iostream>

// Apply callback to each element in container but limit number of in items
// being processed.
// Args:
//      Container -- any contianer that has cbegin(), cend()
//      Callback  -- callable object that accepts (const Value& v, Callback
//      done) ).
template <typename Container, typename Callback>
auto bounded_async_foreach(unsigned n, Container c, Callback cb) {
  using iter_t = decltype(std::cbegin(c));
  struct algorithm_state {
    iter_t it;
    unsigned n;
    Container c;
    Callback cb;
    algorithm_state(Callback&& cb) : cb(std::move(cb)) {}
    std::function<void()> process_next;
  };

  auto shared_state = std::make_shared<algorithm_state>(std::move(cb));

  shared_state->it = std::cbegin(c);
  shared_state->n = n;
  shared_state->c = std::move(c);
  //shared_state->cb = std::move(cb);
  shared_state->process_next = 
      [shared_state_weak = std::weak_ptr<algorithm_state>(shared_state)]() {
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
            shared_state->process_next();
            return;
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
