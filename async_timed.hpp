#include <functional>
#include <type_traits>
#include <iostream>
#include <type_traits>
#include <utility>

int sum(int x, int y) { return x + y; }

int min_overloaded(int x, int y) { return x < y ? x : y; }
int min_overloaded(float x, float y) { return x < y ? x : y; }

template<class Callable>
auto make_timed(Callable&& c, int timeout) {
    return [c=std::forward<Callable>(c)](auto&&... args) {
        c(std::forward<decltype(args)>(args)...);
    };
}

// to wrap async function it should know how to deliver timeout error.
// first of all it cannot be totally abstract without some help, or alternatively,
// we can assume that all callbacks are like void(std::error_code, T), this way
// we can expect callback like that (we can even abstract out this somehow if needed).
template<class DoWork, class Done>
auto async_timed(DoWork&& do_work, Done&& done, int timeout) {
    return [do_work=std::move(do_work), done=std::move(done)](auto&&... args) {
        do_work(std::move(args)..., [done](std::error_code ec) { // todo: here could be overlaoded for one with result.
            // cancel timer.
            done(ec);
        });

        // auto timer = set_timer(timeout, [done]() { done(make_error_code(std::errc::timeout)); });
    };
}

// Don't use this; see discussion.
template<typename Tuple, std::size_t... Ints>
auto select_tuple(Tuple&& tuple, std::index_sequence<Ints...>)
{
 return std::make_tuple(
    std::get<Ints>(std::forward<Tuple>(tuple))...);
}

// lets try to cut off tail of arguments.
template<class Callable>
auto async_timed2(int timeout, Callable&& c) {
    return [c=std::move(c)](auto...args) {        
        std::tuple args_as_tuple{args...};
        // static_assert(typeof last element is_invocable as std::error_code)
        auto& done = std::get<sizeof...(args) - 1>(args_as_tuple);
        auto args_before_done_seq = std::make_index_sequence<sizeof...(args)-1>{};

        auto args_before_done = select_tuple(args_as_tuple, args_before_done_seq);

        auto on_handler = [done](std::error_code ec) {
            // ...
            done(ec);
        };
        std::tuple just_handler(on_handler);
        auto final_args = std::tuple_cat(args_before_done, just_handler);
        std::apply(c, final_args);
    };
}

void test() {
    auto decorated1 = make_timed([](int i1, int i2) {}, 10);
    decorated1(1,2);
    
    std::function<void(int, float)> f = [](int, float) {};
    auto decorated2 = make_timed(std::move(f), 20);
    decorated2(1, 2.0);

    std::function<void(int, float)> f2 = [](int, float) {};
    auto decorated3 = make_timed(f, 20);
    decorated3(1, 2.0);

    make_timed(&sum, 20);

    // make_timed(&min_overloaded, 20); // lift needed here
    make_timed([](auto&&... args){ return min_overloaded(std::forward<decltype(args)>(args)...);}, 20); // manual lift


    // assuming we have some operation
    auto ssl_shutdown = [](int socket, auto done_cb) {
        if (socket == 0) {
            return done_cb(make_error_code(std::errc::io_error));
        }
        done_cb(std::error_code()); // success
    };

    // auto timed_ssl_shutdown = async_timed(std::move(ssl_shutdown), [](std::error_code ec) {
    // }, 10);

    // int socket = 10; // some socket
    // timed_ssl_shutdown(socket);

    auto ssl_shutdown_with_timeout = async_timed2(30, ssl_shutdown);
    
     int socket2 = 10; // some socket
    ssl_shutdown_with_timeout(socket2, [](std::error_code ec) {
        // done!
    });
}

