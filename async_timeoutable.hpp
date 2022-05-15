#pragma once

#include <asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <iostream>
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
auto select_tuple(Tuple&& tuple, std::index_sequence<Ints...>) {
    return std::make_tuple(std::get<Ints>(std::forward<Tuple>(tuple))...);
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
    return [c = std::forward<Callable>(c), &ctx](auto... args) {
        using namespace details;

        // TODO: write correct explanation
        static_assert(sizeof...(args) >= 2);

        std::tuple args_as_tuple{args...};
        auto& timeout_duration = std::get<0>(args_as_tuple);
        auto& done = std::get<sizeof...(args) - 1>(args_as_tuple);

        static_assert(is_duration<std::decay_t<decltype(timeout_duration)>>::value,
                      "first argument expected to be timeout of duration type");

        // spell_type<decltype(done)> s;
        static_assert(std::is_invocable_v<decltype(done), std::error_code> ||
                          std::is_invocable_v<decltype(done), std::error_code, default_val&>,
                      "last argument expected to be callback");

        // cut off first (timeout_duration) and last (callback) argument.
        auto input_args =
            select_tuple(args_as_tuple, offset_sequence_t<1, std::make_index_sequence<sizeof...(args) - 2>>{});

        auto control_block = std::make_shared<control_block_t>(asio::steady_timer(ctx));

        // TODO: get rid of code duplication
        // TODO: forward where appropriate
        // TODO: get rid of done copyability constraint.
        if constexpr (std::is_invocable_v<decltype(done), std::error_code>) {
            control_block->timeout_timer.expires_from_now(timeout_duration);
            control_block->timeout_timer.async_wait([done, control_block](std::error_code ec) {
                if (!ec) {
                    if (!control_block->done_flag) {
                        control_block->timeout_flag = true;
                        done(make_error_code(std::errc::timed_out));
                        return;
                    } else {
                    }
                } else if (ec != asio::error::operation_aborted) {
                    std::cerr << "async_timeoutable: timeout timer error: " << ec.message() << "\n";
                }
            });

            auto on_handler = [done, control_block](std::error_code ec) {
                if (control_block->timeout_flag) {
                    // do nothing
                } else {
                    control_block->done_flag = true;
                    control_block->timeout_timer.cancel();
                    done(ec);
                }
            };
            auto patched_args = std::tuple_cat(input_args, std::tuple(on_handler));
            std::apply(c, patched_args);
        } else {
            control_block->timeout_timer.expires_from_now(timeout_duration);
            control_block->timeout_timer.async_wait([done, control_block](std::error_code ec) {
                if (!ec) {
                    if (!control_block->done_flag) {
                        control_block->timeout_flag = true;
                        done(make_error_code(std::errc::timed_out), default_val{});
                        return;
                    } else {
                    }
                } else if (ec != asio::error::operation_aborted) {
                    std::cerr << "async_timeoutable: timeout timer error: " << ec.message() << "\n";
                }
            });

            auto on_handler = [done, control_block](std::error_code ec, auto r) {
                if (control_block->timeout_flag) {
                    // do nothing
                } else {
                    control_block->done_flag = true;
                    control_block->timeout_timer.cancel();
                    done(ec, r);
                }
            };

            auto patched_args = std::tuple_cat(input_args, std::tuple(on_handler));
            std::apply(c, patched_args);
        }
    };
}