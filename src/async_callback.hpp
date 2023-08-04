#pragma once

#include <fmt/format.h>
#include <cassert>
#include <experimental/source_location>
#include <function2/function2.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
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

struct default_log_fns_t {
    template <class... Args>
    static void print_error_line(std::string_view fmt_str, Args&&... args) {
        std::cerr << "ERROR: " << fmt::vformat(fmt_str, fmt::make_format_args(args...)) << "\n";
    }
};

template <class T, class LogFns = default_log_fns_t>
class async_callback_impl_t {
   public:
    async_callback_impl_t() : m_cb(nullptr) {}

    template <class Callable>
    async_callback_impl_t(Callable cb,
                          std::experimental::source_location sloc = std::experimental::source_location::current())
        : m_cb(std::move(cb)), m_origin_tag(fmt::format("{}:{}", sloc.file_name(), sloc.line())) {}

    ~async_callback_impl_t() { handle_not_called(); }

    async_callback_impl_t(const async_callback_impl_t&) = delete;
    async_callback_impl_t& operator=(const async_callback_impl_t&) = delete;

    async_callback_impl_t(async_callback_impl_t&& rhs) {
        m_cb = std::move(rhs.m_cb);
        m_called = rhs.m_called;
        rhs.m_cb = nullptr;  // TODO: try swap/exhange instead once we have test that covers this.
    }

    async_callback_impl_t& operator=(async_callback_impl_t&& rhs) {
        handle_not_called();
        m_cb = std::move(rhs.m_cb);
        m_called = rhs.m_called;
        rhs.m_cb = nullptr;
        return *this;
    }

    template <typename EnableWhenVoid = std::enable_if<std::is_same_v<T, void>>,
              typename = typename EnableWhenVoid::type>
    void operator()(const std::error_code& ec) {
        call_operator(ec);
    }

    template <typename Param = T,
              typename EnableWhenNonVoid = std::enable_if<!std::is_same_v<T, void>>,
              typename = typename EnableWhenNonVoid::type>
    void operator()(const std::error_code& ec, Param&& p) {
        call_operator(ec, std::forward<Param>(p));
    }

    operator bool() const { return m_cb.operator bool(); }

   private:
    template <class... Args>
    void call_operator(Args&&... args) {
        static_assert(std::is_invocable_v<decltype(m_cb), Args...>, "not invocable");

        if (!m_called) {
            m_called = true;
            std::invoke(m_cb, std::forward<Args>(args)...);
        } else {
            LogFns::print_error_line("async_kit: {}: attempt to call the callback twice", m_origin_tag);
            return;
        }
    }

    void handle_not_called() {
        if (m_cb) {
            if (!m_called) {
                LogFns::print_error_line("async_kit: {}: callback has not been called", m_origin_tag);
                if constexpr (std::is_invocable_v<decltype(m_cb), std::error_code>) {
                    std::exchange(m_cb, nullptr)(make_error_code(errors::async_callback_err::not_called));
                } else {
                    std::exchange(m_cb, nullptr)(make_error_code(errors::async_callback_err::not_called), {});
                }
            }
        }
    }

   private:
    typename func_type<T>::async_callback m_cb;
    bool m_called = false;
    std::string m_origin_tag;
};

// TODO: ensure async_callback cannot be created from shared one to prevent.
template <class T>
class shared_async_callback_impl_t {
   public:
    explicit shared_async_callback_impl_t(async_callback_impl_t<T> cb)
        : m_cb_shared_ptr(std::make_shared<async_callback_impl_t<T>>(std::move(cb))) {}

    template <typename EnableWhenVoid = std::enable_if<std::is_same_v<T, void>>,
              typename = typename EnableWhenVoid::type>
    void operator()(const std::error_code& ec) {
        assert(m_cb_shared_ptr);
        (*m_cb_shared_ptr)(ec);
    }

    template <typename Param = T,
              typename EnableWhenNonVoid = std::enable_if<!std::is_same_v<T, void>>,
              typename = typename EnableWhenNonVoid::type>
    void operator()(const std::error_code& ec, Param&& p) {
        assert(m_cb_shared_ptr);
        (*m_cb_shared_ptr)(ec, std::forward<Param>(p));
    }

    operator bool() const { return m_cb_shared_ptr->operator bool(); }

   private:
    std::shared_ptr<async_callback_impl_t<T>> m_cb_shared_ptr;
};

template <class T>
shared_async_callback_impl_t<T> to_shared(async_callback_impl_t<T> cb) {
    return shared_async_callback_impl_t(std::move(cb));
}

}  // namespace lsem::async_kit