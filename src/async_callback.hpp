#pragma once

#include <function2/function2.hpp>
#include <system_error>

namespace lsem::async_kit {
namespace errors {
enum class async_callback_err {
    // no 0
    not_called = 1,
};

std::error_code make_error_code(async_callback_err e);

}  // namespace errors
}  // namespace lsem::async_kit

namespace std {
template <>
struct is_error_code_enum<lsem::async_kit::errors::async_callback_err> : true_type {};
}  // namespace std

namespace lsem::async_kit {

template <class T>
struct func_type {
    using async_callback = fu2::unique_function<void(std::error_code, T)>;
};

template <>
struct func_type<void> {
    using async_callback = fu2::unique_function<void(std::error_code)>;
};

template <class T>
using async_callback = typename func_type<T>::async_callback;

}  // namespace lsem::async_kit