#pragma once

#include <asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <optional>
#include <system_error>
#include <type_traits>
#include <utility>

namespace details {
template <std::size_t N, typename Seq>
struct offset_sequence;

template <std::size_t N, std::size_t... Ints>
struct offset_sequence<N, std::index_sequence<Ints...>> {
    using type = std::index_sequence<Ints + N...>;
};
template <std::size_t N, typename Seq>
using offset_sequence_t = typename offset_sequence<N, Seq>::type;

template <class T>
struct is_duration : std::false_type {};
template <class Rep, class Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

// passpartu
struct default_val {
    template <class T>
    operator T() const {
        return T{};
    }
};

template <typename Tuple, std::size_t... Ints>
auto select_tuple_of_refs(Tuple&& tuple, std::index_sequence<Ints...>) {
    return std::forward_as_tuple(std::get<Ints>(std::forward<Tuple>(tuple))...);
}

template <class T>
struct spell_type;

struct control_block_t {
    bool timeout_flag = false;
    bool done_flag = false;
    asio::steady_timer timeout_timer;

    control_block_t(asio::steady_timer timeout_timer) : timeout_timer(std::move(timeout_timer)) {}
};

}  // namespace details

// TODO: write documentation and usage examples.
template <class Callable>
auto async_timeoutable(asio::io_context& ctx, Callable&& c) {
    return [c = std::forward<Callable>(c), &ctx](auto&&... args) {
        using namespace details;

        // We expect the following layout of arguments: (timeout, a1, a2, ..., an, done_cb);
        /// having that we need to extract (a1, a2, ..., an), append our own version of done_cb
        // and pass it to callable. a1, a2, ..., an should be forwarded correctly.

        static_assert(sizeof...(args) >= 2,
                      "at least timeout and done_cb expected (expected signature: void(timeout, args...,  done_cb))");

        auto args_as_tuple = std::forward_as_tuple(std::forward<decltype(args)>(args)...);

        auto timeout_duration = std::get<0>(args_as_tuple);
        auto done = std::get<sizeof...(args) - 1>(std::move(args_as_tuple));

        static_assert(is_duration<std::decay_t<decltype(timeout_duration)>>::value,
                      "first argument expected to be timeout of duration type");
        static_assert(std::is_invocable_v<decltype(done), std::error_code> ||
                          std::is_invocable_v<decltype(done), std::error_code, default_val&>,
                      "last argument expected to be callback");

        const auto input_args_indices = offset_sequence_t<1, std::make_index_sequence<sizeof...(args) - 2>>{};
        auto input_args = select_tuple_of_refs(std::move(args_as_tuple), input_args_indices);

        // TODO: employ the same trick with control bloc as we do with done. this way we can get rid of boolean
        // variables. first who comes, assigns nullopt and this is indication that it is second. this would also free
        // resources.

        auto control_block = std::make_shared<control_block_t>(asio::steady_timer(ctx));

        // TODO: get rid of code duplication
        if constexpr (std::is_invocable_v<decltype(done), std::error_code>) {
            auto done_ptr = std::make_shared<std::optional<decltype(done)>>(std::move(done));

            control_block->timeout_timer.expires_from_now(timeout_duration);
            control_block->timeout_timer.async_wait([done_ptr, control_block](std::error_code ec) {
                if (!ec) {
                    if (!control_block->done_flag) {
                        control_block->timeout_flag = true;
                        (**done_ptr)(make_error_code(std::errc::timed_out));
                        // free resources
                        *done_ptr = std::nullopt;
                        return;
                    }
                } else if (ec != asio::error::operation_aborted) {
                    std::cerr << "async_timeoutable: timeout timer error: " << ec.message() << "\n";
                }
            });

            auto patched_handler = [done_ptr, control_block](std::error_code ec) {
                if (!control_block->timeout_flag) {
                    control_block->done_flag = true;
                    // TODO: free control block instead of cancelling
                    control_block->timeout_timer.cancel();
                    (**done_ptr)(ec);
                    *done_ptr = std::nullopt;
                }
            };
            auto patched_args = std::tuple_cat(std::move(input_args), std::tuple(patched_handler));
            std::apply(c, std::move(patched_args));
        } else {
            auto done_ptr = std::make_shared<std::optional<decltype(done)>>(std::move(done));

            control_block->timeout_timer.expires_from_now(timeout_duration);
            control_block->timeout_timer.async_wait([done_ptr, control_block](std::error_code ec) {
                if (!ec) {
                    if (!control_block->done_flag) {
                        control_block->timeout_flag = true;
                        (**done_ptr)(make_error_code(std::errc::timed_out), default_val{});
                        // free resources
                        *done_ptr = std::nullopt;
                        return;
                    }
                } else if (ec != asio::error::operation_aborted) {
                    std::cerr << "async_timeoutable: timeout timer error: " << ec.message() << "\n";
                }
            });

            auto patched_handler = [done_ptr, control_block](std::error_code ec, auto&& r) {
                if (!control_block->timeout_flag) {
                    control_block->done_flag = true;
                    // TODO: free control block instead of cancelling
                    control_block->timeout_timer.cancel();
                    (**done_ptr)(ec, std::forward<decltype(r)>(r));
                    *done_ptr = std::nullopt;
                }
            };

            auto patched_args = std::tuple_cat(std::move(input_args), std::tuple(patched_handler));
            std::apply(c, std::move(patched_args));
        }
    };
}