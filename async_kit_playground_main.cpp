#include "call_monitor.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

template <class T>
struct result_type : public result_type<decltype(&T::operator())> {};

template <class ClassType, class T>
struct result_type<void (ClassType::*)(std::error_code, T) const> {
    using type = T;
};

template <class ClassType>
struct result_type<void (ClassType::*)(std::error_code) const> {
    using type = void;
};

template <class ClassType, class T>
struct result_type<void (ClassType::*)(std::error_code, T)> {
    using type = T;
};

template <class ClassType>
struct result_type<void (ClassType::*)(std::error_code)> {
    using type = void;
};

template <typename AsioLikeCallable>
using result_type_t = typename result_type<AsioLikeCallable>::type;
#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

#define LIFT(X) \
    [](auto&&... args) noexcept(noexcept(X(FWD(args)...))) -> decltype(X(FWD(args)...)) { return X(FWD(args)...); }

using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////////////
template <std::size_t N, typename Seq>
struct offset_sequence;

template <std::size_t N, std::size_t... Ints>
struct offset_sequence<N, std::index_sequence<Ints...>> {
    using type = std::index_sequence<Ints + N...>;
};
template <std::size_t N, typename Seq>
using offset_sequence_t = typename offset_sequence<N, Seq>::type;
////////////////////////////////////////////////////////////////////////////////

template <typename Callback>
void async_sleep(asio::io_context& ctx, std::chrono::steady_clock::duration t, Callback&& cb) {
    auto timer = std::make_shared<asio::steady_timer>(ctx, t);
    timer->async_wait([timer, cb = std::move(cb)](std::error_code ec) { cb(ec); });
}

template <typename Tuple, std::size_t... Ints>
auto select_tuple_of_refs(Tuple&& tuple, std::index_sequence<Ints...>) {
    return std::forward_as_tuple(std::get<Ints>(std::forward<Tuple>(tuple))...);
}

template <class T>
struct spell_type;

// passpartu
struct default_val {
    template <class T>
    operator T() const {
        return T{};
    }
};

struct control_block {
    int attempt = 1;
    const int attempts_limit = 3;
    control_block(std::function<void(std::function<void(std::error_code)>)> thunk_f) : thunk_f(std::move(thunk_f)) {}
    std::function<void(std::function<void(std::error_code)>)> thunk_f;
    std::function<void(std::error_code)> handler;
};

template <class R>
struct control_block_with_result {
    int attempt = 1;
    const int attempts_limit = 3;

    using callback_type = std::function<void(std::error_code, R)>;
    using thunk_type = std::function<void(callback_type)>;

    control_block_with_result(thunk_type thunk_f) : thunk_f(std::move(thunk_f)) {}
    thunk_type thunk_f;
    callback_type handler;
};

async_scan(1,2,3,4,5, [](std::error_code ec) {
    // 
});

async_scan(1,2,3,4,5, [](std::error_code ecm scan_results r) {});

template <class AsyncFunction>
auto async_retry(asio::io_context& ctx, AsyncFunction&& f) {
    return [f = std::move(f), &ctx](auto&&... args) {
        static_assert(std::is_invocable_v<AsyncFunction, decltype(args)...>);

        auto args_as_tuple = std::forward_as_tuple(std::forward<decltype(args)>(args)...);
        auto done = std::get<sizeof...(args) - 1>(std::move(args_as_tuple));

        const auto input_args_indices = std::make_index_sequence<sizeof...(args) - 1>{};
        auto input_args = select_tuple_of_refs(std::move(args_as_tuple), input_args_indices);

        static_assert(std::is_invocable_v<decltype(done), std::error_code> ||
                          std::is_invocable_v<decltype(done), std::error_code, default_val&>,
                      "last argument expected to be callback");

        if constexpr (std::is_invocable_v<decltype(done), std::error_code>) {
            std::function<void(std::function<void(std::error_code)>)> thunk_f =
                [f = std::move(f), input_args = std::move(input_args)](std::function<void(std::error_code)> cb) {
                    auto all_args = std::tuple_cat(std::move(input_args), std::tuple(cb));
                    std::apply(f, std::move(all_args));
                };

            auto shared_control_block = std::make_shared<control_block>(std::move(thunk_f));

            shared_control_block->handler = [&ctx, done = std::move(done),
                                             shared_control_block](std::error_code ec) mutable {
                auto free_resources_async = [&ctx](std::shared_ptr<control_block> shared_control_block) {
                    ctx.post([shared_control_block] { shared_control_block->handler = nullptr; });
                };

                if (ec) {
                    std::cout << "dbg: failed with error code: " << ec.message() << "\n";
                    if (shared_control_block->attempt++ == shared_control_block->attempts_limit) {
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
        }
    };
}

template <class Handler>
void async_scan(asio::io_context& ctx, Handler cb) {
    // scan simulation (duration 1s)
    async_sleep(ctx, 1s, [cb = std::move(cb)](std::error_code ec) {
        if (rand() % 3 == 0) {
            cb(std::error_code());
        } else {
            cb(make_error_code(std::errc::io_error));
        }
    });
}

void test(std::chrono::duration d) {

}

template<Handler>
transform(Iterator, Iterator, Iterator, Handler) {}

template<class C>
void to_upper(C c) {

}

// to_upper<char>;
// to_upper<wchar_t>

int main() {
    string s;
    std::transform(s.begin(), s.end(), s.begin(), [](char c) { return std::toupper(c); });
    std::transform(s.begin(), s.end(), s.begin(), std::toupper);

    srand(time(NULL));

    asio::io_context ctx;


    test(1s);

    scan_ops sops;    

    auto async_scan_with_retry = async_retry(ctx, LIFT(async_scan));
    async_scan_with_retry(ctx, [](std::error_code ec) {
        std::cout << "async_scan_with_retry done with result: " << ec.message() << "\n";
    });

    ctx.run();
}
