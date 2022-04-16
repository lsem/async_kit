#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <system_error>

template <typename Container, typename Callback, typename FinishCallback>
void bounded_async_foreach(unsigned limit_n, Container c, Callback cb, FinishCallback finished_cb) {
    if (limit_n == 0) {
        finished_cb(make_error_code(std::errc::no_child_process));
        return;
    }

    using iter_type = decltype(std::cbegin(c));
    struct self_state {
        Container c;
        iter_type it;
        Callback cb;
        unsigned free_workers;
        const unsigned n_workers;
        FinishCallback finished_cb;
        std::error_code exec_result = std::error_code();
        std::function<void()> process_next;

        self_state(Container c, Callback cb, unsigned n, FinishCallback finished_cb)
            : c(std::move(c)),
              it(std::cbegin(this->c)),
              cb(std::move(cb)),
              free_workers(n),
              n_workers(n),
              finished_cb(std::move(finished_cb)) {}
    };

    auto self = std::make_shared<self_state>(std::move(c), std::move(cb), limit_n, std::move(finished_cb));

    self->process_next = [self]() {
        if (self->it != std::cend(self->c)) {
            if (self->free_workers > 0) {
                self->free_workers--;
                self->cb(*self->it++, [self](std::error_code ec) {
                    self->free_workers++;
                    if (ec) {
                        // first error sticks to state, other errors basically ignored,
                        // this targets practical use case for cancelling.
                        if (!self->exec_result) {
                            self->exec_result = ec;
                        }
                        self->it = std::cend(self->c);
                    } else {
                        // success, just go on to next one.
                    }
                    self->process_next();
                });
                self->process_next();
            }
        } else {
            // last element tries to find new work, non-last just fade out.
            if (self->free_workers == self->n_workers) {
                self->finished_cb(self->exec_result);
            }
        }
    };

    self->process_next();
}
