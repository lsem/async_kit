#pragma once

#include <functional>
#include <memory>

// Apply callback to each element in container but limit number of in items
// being processed.
// Args:
//      Container -- any contianer that has cbegin(), cend()
//      Callback  -- callable object that accepts (const Value& v, Callback
//      done) ).
template <typename Container, typename Callback>
auto bounded_async_foreach(unsigned n, const Container &c, Callback &&cb) {
  using iter_t = decltype(std::cbegin(c));
  struct algorithm_state {
    iter_t it;
    unsigned n = 0;
    algorithm_state(iter_t it, unsigned n) : it(it), n(n) {}
  };

  auto shared_state = std::make_shared<algorithm_state>(std::cbegin(c), n);
  auto process_next = std::make_shared<std::function<void()>>(nullptr);

  *process_next = [process_next,
                   shared_state_weak =
                       std::weak_ptr<algorithm_state>(shared_state),
                   &cb, end_it = std::cend(c)]() {
    if (auto shared_state = shared_state_weak.lock()) {
      if (shared_state->it == end_it) {
        return;
      }
      if (shared_state->n > 0) {
        shared_state->n--;
        cb(*shared_state->it++, [process_next, shared_state_weak]() {
          if (auto shared_state = shared_state_weak.lock()) {
            // free one vacant place and start next one if there is any
            shared_state->n++;
            (*process_next)();
            return;
          } else {
            std::cout << "(dbg): cancalled\n";
          }
        });
        // process next if there is vacant place
        (*process_next)();
      } else {
        // there are not vacant palces
      }
    } else {
      std::cout << "(dbg): cancalled\n";
    }
  };

  (*process_next)();

  auto ctx = [shared_state]() mutable { shared_state.reset(); };
  return ctx;
}
