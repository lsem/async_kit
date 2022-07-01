#include "utils.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <functional>
#include <iostream>  // todo: get rid of this.

namespace lsem::async {

using namespace std::chrono_literals;

template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::move(cb)](std::error_code ec) { cb(ec); });
}

struct async_retry_opts {
    unsigned attempts = 3u;
    std::chrono::steady_clock::duration pause = 1s;
};

struct control_block {
    int attempt = 1;
    const async_retry_opts opts;

    control_block(std::function<void(std::function<void(std::error_code)>)> thunk_f, async_retry_opts opts)
        : thunk_f(std::move(thunk_f)), opts(std::move(opts)) {}

    std::function<void(std::function<void(std::error_code)>)> thunk_f;
    std::function<void(std::error_code)> handler;
};

template <class R>
struct control_block_with_result {
    int attempt = 1;
    const async_retry_opts opts;

    using callback_type = std::function<void(std::error_code, R)>;
    using thunk_type = std::function<void(callback_type)>;

    control_block_with_result(thunk_type thunk_f, async_retry_opts opts)
        : thunk_f(std::move(thunk_f)), opts(std::move(opts)) {}

    thunk_type thunk_f;
    callback_type handler;
};

template <class AsyncFunction>
auto async_retry(asio::io_context& ctx, AsyncFunction&& f, async_retry_opts opts = {}) {
    return [f = std::move(f), opts = std::move(opts), &ctx](auto&&... args) {
        static_assert(std::is_invocable_v<AsyncFunction, decltype(args)...>);

        auto args_as_tuple = std::forward_as_tuple(std::forward<decltype(args)>(args)...);
        auto done = std::get<sizeof...(args) - 1>(std::move(args_as_tuple));

        const auto input_args_indices = std::make_index_sequence<sizeof...(args) - 1>{};
        auto input_args = utils::select_tuple_of_refs(std::move(args_as_tuple), input_args_indices);

        static_assert(std::is_invocable_v<decltype(done), std::error_code> ||
                          std::is_invocable_v<decltype(done), std::error_code, utils::default_val&>,
                      "last argument expected to be callback");

        if constexpr (std::is_invocable_v<decltype(done), std::error_code>) {
            std::function<void(std::function<void(std::error_code)>)> thunk_f =
                [f = std::move(f), input_args = std::move(input_args)](std::function<void(std::error_code)> cb) {
                    auto all_args = std::tuple_cat(std::move(input_args), std::tuple(cb));
                    std::apply(f, std::move(all_args));
                };

            auto shared_control_block = std::make_shared<control_block>(std::move(thunk_f), std::move(opts));

            shared_control_block->handler = [&ctx, done = std::move(done),
                                             shared_control_block](std::error_code ec) mutable {
                auto free_resources_async = [&ctx](std::shared_ptr<control_block> shared_control_block) {
                    ctx.post([shared_control_block] { shared_control_block->handler = nullptr; });
                };

                if (ec) {
                    std::cout << "dbg: failed with error code: " << ec.message() << "\n";
                    if (shared_control_block->attempt++ == shared_control_block->opts.attempts) {
                        std::cout << "dbg: attempts limit reached\n";
                        free_resources_async(shared_control_block);
                        done(ec);  // q: only last error, what about others?
                        return;
                    } else {
                        // use 1s sleep before doing next attemp.
                        std::cout << "dbg: next attempt will take place in 1s\n";
                        async_sleep(ctx, 1s, [shared_control_block](std::error_code ec) {
                            std::cout << "dbg: attempt " << shared_control_block->attempt << "..\n";
                            shared_control_block->thunk_f(shared_control_block->handler);
                        });
                        return;
                    }
                } else {
                    std::cout << "dbg: success\n";
                    free_resources_async(shared_control_block);
                    done(ec);
                }
            };

            std::cout << "dbg: attempt " << shared_control_block->attempt << "..\n";
            shared_control_block->thunk_f(shared_control_block->handler);
        } else {
            // TODO: get rid of code duplication

            using async_result_type = utils::result_type_t<decltype(done)>;

            std::function<void(std::function<void(std::error_code, async_result_type)>)> thunk_f =
                [f = std::move(f),
                 input_args = std::move(input_args)](std::function<void(std::error_code, async_result_type)> cb) {
                    auto all_args = std::tuple_cat(std::move(input_args), std::tuple(cb));
                    std::apply(f, std::move(all_args));
                };

            auto shared_control_block =
                std::make_shared<control_block_with_result<async_result_type>>(std::move(thunk_f), std::move(opts));

            shared_control_block->handler = [&ctx, done = std::move(done), shared_control_block](
                                                std::error_code ec, async_result_type r) mutable {
                auto free_resources_async =
                    [&ctx](std::shared_ptr<control_block_with_result<async_result_type>> shared_control_block) {
                        ctx.post([shared_control_block] { shared_control_block->handler = nullptr; });
                    };

                if (ec) {
                    std::cout << "dbg: failed with error code: " << ec.message() << "\n";
                    if (shared_control_block->attempt++ == shared_control_block->opts.attempts) {
                        std::cout << "dbg: attempts limit reached\n";
                        free_resources_async(shared_control_block);
                        done(ec, async_result_type{});  // q: only last error, what about others?
                        return;
                    } else {
                        // use 1s sleep before doing next attemp.
                        std::cout << "dbg: next attempt will take place in 1s\n";
                        async_sleep(ctx, 1s, [shared_control_block](std::error_code ec) {
                            std::cout << "dbg: attempt " << shared_control_block->attempt << "..\n";
                            shared_control_block->thunk_f(shared_control_block->handler);
                        });
                        return;
                    }
                } else {
                    std::cout << "dbg: success\n";
                    free_resources_async(shared_control_block);
                    done(ec, r);
                }
            };
            std::cout << "dbg: attempt " << shared_control_block->attempt << "..\n";
            shared_control_block->thunk_f(shared_control_block->handler);
        }
    };
}

}  // namespace lsem::async