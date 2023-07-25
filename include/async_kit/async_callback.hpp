#pragma once

#include <function2/function2.hpp>
#include <system_error>

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
