#pragma once

#include <function2/function2.hpp>
#include <iostream>
#include <memory>
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
    static void print_error_line(const char* msg) { std::cerr << "ERROR: " << msg << "\n"; }
};

template <class T, class LogFns = default_log_fns_t>
class async_callback_impl_t {
   public:
    async_callback_impl_t() : m_cb(nullptr) {}

    template <class Callable>
    async_callback_impl_t(Callable cb) : m_cb(std::move(cb)) {}

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
        if (!m_called) {
            m_called = true;
            m_cb(ec);
        } else {
            LogFns::print_error_line("async_kit: attempt to call the callback twice");
            return;
        }
    }

    template <typename Param = T,
              typename EnableWhenNonVoid = std::enable_if<!std::is_same_v<T, void>>,
              typename = typename EnableWhenNonVoid::type>
    void operator()(const std::error_code& ec, Param&& p) {
        if (!m_called) {
            m_called = true;
            m_cb(ec, std::forward<Param>(p));
        } else {
            LogFns::print_error_line("async_kit: attempt to call the callback twice");
            return;
        }
    }

    operator bool() const { return m_cb.operator bool(); }

   private:
    void handle_not_called() {
        if (m_cb) {
            if (!m_called) {
                LogFns::print_error_line("async_kit: callback not called");

                // TODO: this is working only or void callback.
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
};

}  // namespace lsem::async_kit