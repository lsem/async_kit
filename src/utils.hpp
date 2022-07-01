#pragma once

#include <system_error>
#include <tuple>
#include <type_traits>

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

#define LIFT(X) \
    [](auto&&... args) noexcept(noexcept(X(FWD(args)...))) -> decltype(X(FWD(args)...)) { return X(FWD(args)...); }

namespace lsem::async::utils {

template <class T>
struct spell_type;

// passpartu
struct default_val {
    template <class T>
    operator T() const {
        return T{};
    }
};

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

template <typename Tuple, std::size_t... Ints>
auto select_tuple_of_refs(Tuple&& tuple, std::index_sequence<Ints...>) {
    return std::forward_as_tuple(std::get<Ints>(std::forward<Tuple>(tuple))...);
}

}  // namespace lsem::async::utils