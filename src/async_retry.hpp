#include "utils.hpp"

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>

#include <function2/function2.hpp>
#include <iostream>

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

namespace details {

struct control_block {
    int attempt = 1;
    const async_retry_opts opts;

    using callback_type = fu2::unique_function<void(std::error_code)>;
    using thunk_type = fu2::unique_function<void()>;

    explicit control_block(async_retry_opts opts, asio::steady_timer pause_timer)
        : opts(std::move(opts)), pause_timer(std::move(pause_timer)) {}

    void cancel() { pause_timer.cancel(); }

    thunk_type try_next;
    callback_type custom_handler;
    std::shared_ptr<fu2::unique_function<void()>> real_cancel;
    asio::steady_timer pause_timer;
};

template <class R>
struct control_block_with_result {
    using callback_type = fu2::unique_function<void(std::error_code, R)>;
    using thunk_type = fu2::unique_function<void()>;

    explicit control_block_with_result(async_retry_opts opts, asio::steady_timer pause_timer)
        : opts(std::move(opts)), pause_timer(std::move(pause_timer)) {}

    void cancel() { pause_timer.cancel(); }

    int attempt = 1;
    const async_retry_opts opts;
    thunk_type try_next;
    callback_type custom_handler;
    std::shared_ptr<fu2::unique_function<void()>> real_cancel;
    asio::steady_timer pause_timer;
};
}  // namespace details

// token is a thing that allow to cancel async operation.
struct cancellation_token {
    ~cancellation_token() {
        if (impl) {
            impl();
        }
    }

    void cancel() {
        if (impl) {
            impl();
            impl = nullptr;
        }
    }

    // TODO: cancel_async(done)?

    // implementors of async functions can hook they implementation specific
    // cancellation mechanism to be invoked when user wants to cancel something.
    // implementation can put anything callable into impl and that will be invoked either by calling function
    // or in destrcutor automatically.
    fu2::unique_function<void()> impl;
};

namespace {
cancellation_token g_dummy_token;
}


template <class AsyncFunction>
auto async_retry(asio::io_context& ctx,
                 AsyncFunction&& f,
                 async_retry_opts opts = {},
                 cancellation_token& token_ref = g_dummy_token) {
    if (opts.attempts == 0) {
        throw std::invalid_argument("0 attempts not supported by async_retry");
    }

    auto real_cancel = std::make_shared<fu2::unique_function<void()>>();
    token_ref.impl = [real_cancel] {
        if (*real_cancel) {
            (*real_cancel)();
        } else {
            // not started or already destroyed.
        }
    };

    return [f = std::move(f), opts = std::move(opts), &ctx, real_cancel](auto&&... args) {
        static_assert(std::is_invocable_v<AsyncFunction, decltype(args)...>);

        auto args_as_tuple = std::forward_as_tuple(std::forward<decltype(args)>(args)...);

        auto done = std::get<sizeof...(args) - 1>(std::move(args_as_tuple));

        const auto input_args_indices = std::make_index_sequence<sizeof...(args) - 1>{};
        auto input_args = utils::select_tuple_of_refs(std::move(args_as_tuple), input_args_indices);

        static_assert(std::is_invocable_v<decltype(done), std::error_code> ||
                          std::is_invocable_v<decltype(done), std::error_code, utils::default_val&>,
                      "last argument expected to be callback");

        if constexpr (std::is_invocable_v<decltype(done), std::error_code>) {
            auto shared_control_block =
                std::make_shared<details::control_block>(std::move(opts), asio::steady_timer{ctx});

            *real_cancel = [shared_control_block]() { shared_control_block->cancel(); };
            shared_control_block->real_cancel = real_cancel;

            shared_control_block->try_next = [shared_control_block, f = std::move(f),
                                              input_args = std::move(input_args)]() {
                // we cannot move our own handler because we need it for more than one attempt so we create
                // one more wrapper.
                auto wrapper = [shared_control_block](std::error_code ec) { shared_control_block->custom_handler(ec); };
                auto all_args = std::tuple_cat(std::move(input_args), std::tuple{wrapper});
                std::apply(f, std::move(all_args));
            };

            shared_control_block->custom_handler = [&ctx, done = std::move(done),
                                                    shared_control_block](std::error_code ec) mutable {
                auto free_resources_async = [&ctx](std::shared_ptr<details::control_block> shared_control_block) {
		    asio::post(ctx, [shared_control_block] {
                        shared_control_block->custom_handler = nullptr;
                        shared_control_block->try_next = nullptr;
                        *shared_control_block->real_cancel = nullptr;
                        shared_control_block->real_cancel = nullptr;
                    });
                };

                if (ec) {
                    if (shared_control_block->attempt++ == shared_control_block->opts.attempts) {
                        free_resources_async(shared_control_block);
                        done(ec);  // q: only last error, what about others?
                        return;
                    } else {
                        shared_control_block->pause_timer.expires_after(shared_control_block->opts.pause);
                        shared_control_block->pause_timer.async_wait(
                            [free_resources_async, shared_control_block, done = std::move(done)](std::error_code ec) {
                                if (!ec) {
                                    shared_control_block->try_next();
                                } else {
                                    std::cout << "pause timer error: " << ec.message() << "\n";
                                    free_resources_async(shared_control_block);
                                    done(ec);
                                }
                            });
                        return;
                    }
                } else {
                    free_resources_async(shared_control_block);
                    done(ec);
                }
            };

            shared_control_block->try_next();
        } else {
            // TODO: get rid of code duplication

            using async_result_type = utils::result_type_t<decltype(done)>;

            auto shared_control_block = std::make_shared<details::control_block_with_result<async_result_type>>(
                std::move(opts), asio::steady_timer{ctx});

            *real_cancel = [shared_control_block]() { shared_control_block->cancel(); };
            shared_control_block->real_cancel = real_cancel;

            shared_control_block->try_next = [shared_control_block, f = std::move(f),
                                              input_args = std::move(input_args)]() {
                // we cannot move our own handler because we need it for more than one attempt so we create
                // one more wrapper.
                auto wrapper = [shared_control_block](std::error_code ec, async_result_type r) {
                    shared_control_block->custom_handler(ec, std::move(r));
                };
                auto all_args = std::tuple_cat(std::move(input_args), std::tuple{wrapper});
                std::apply(f, std::move(all_args));
            };

            shared_control_block->custom_handler = [&ctx, done = std::move(done), shared_control_block](
                                                       std::error_code ec, async_result_type r) mutable {
                auto free_resources_async =
                    [&ctx](
                        std::shared_ptr<details::control_block_with_result<async_result_type>> shared_control_block) {
			asio::post(ctx, [shared_control_block] {
                            shared_control_block->custom_handler = nullptr;
                            shared_control_block->try_next = nullptr;
                            *shared_control_block->real_cancel = nullptr;
                            shared_control_block->real_cancel = nullptr;
                        });
                    };

                if (ec) {
                    if (shared_control_block->attempt++ == shared_control_block->opts.attempts) {
                        free_resources_async(shared_control_block);
                        done(ec, async_result_type{});  // q: only last error, what about others?
                        return;
                    } else {
                        shared_control_block->pause_timer.expires_after(shared_control_block->opts.pause);
                        shared_control_block->pause_timer.async_wait(
                            [free_resources_async, shared_control_block, done = std::move(done)](std::error_code ec) {
                                if (!ec) {
                                    shared_control_block->try_next();
                                } else {
                                    std::cout << "pause timer error: " << ec.message() << "\n";
                                    free_resources_async(shared_control_block);
                                    done(ec, async_result_type{});  // q: only last error, what about others?
                                }
                            });
                        return;
                    }
                } else {
                    free_resources_async(shared_control_block);
                    done(ec, std::move(r));
                }
            };

            shared_control_block->try_next();
        }
    };
}

}  // namespace lsem::async
