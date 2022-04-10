#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Apply callback to each element in container.
//      Container -- any contianer that has cbegin(), cend()
//      Callback  -- callable object that accepts (const Value& v, Callback
//      done) ).
template <typename Container, typename Callback>
void bounded_async_foreach(unsigned n, const Container &c, Callback &&cb) {

  using iter_t = decltype(std::cbegin(c));
  struct algorithm_state {
    iter_t it;
    unsigned n = 0;

    algorithm_state(iter_t it, unsigned n) : it(it), n(n) {}
  };

  auto shared_state = std::make_shared<algorithm_state>(std::cbegin(c), n);

  auto process_next = std::make_shared<std::function<void()>>(nullptr);
  *process_next = [process_next, shared_state, &cb, end_it = std::cend(c)]() {
    if (shared_state->it == end_it) {
      return;
    }
    if (shared_state->n > 0) {
      shared_state->n--; // consume one vacant place
      cb(*shared_state->it++, [process_next, shared_state]() {
        shared_state->n++; // free one vacant place
        (*process_next)();
        return;
      });
      (*process_next)(); // start next one immidiately  if there is vacant place
                         // in pool.
    } else {
      // there are not vacant palces
    }
  };

  (*process_next)();
}
