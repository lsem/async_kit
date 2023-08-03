#include "async_callback.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace errors = lsem::async_kit::errors;

template <class T>
using tested_async_callback = lsem::async_kit::async_callback_impl_t<T>;

using testing::ElementsAre;

TEST(async_callback_test, basic_test__not_called__void) {
    std::vector<std::error_code> invocations;
    {
        tested_async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        EXPECT_TRUE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}

TEST(async_callback_test, basic_test__not_called__param) {
    std::vector<std::pair<std::error_code, int>> invocations;
    {
        tested_async_callback<int> cb = [&](std::error_code ec, int arg) { invocations.emplace_back(ec, arg); };
        EXPECT_TRUE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(std::pair<std::error_code, int>{
                                 make_error_code(errors::async_callback_err::not_called), int{}}));
}

TEST(async_callback_test, calling_with_default_initialier__void) {
    std::vector<std::error_code> invocations;
    tested_async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
    cb({});
    EXPECT_THAT(invocations, ElementsAre(std::error_code{}));
}

TEST(async_callback_test, calling_with_default_initialier__param) {
    std::vector<std::pair<std::error_code, int>> invocations;
    tested_async_callback<int> cb = [&](std::error_code ec, int arg) { invocations.emplace_back(ec, arg); };
    cb({}, 11);
    EXPECT_THAT(invocations, ElementsAre(std::pair<std::error_code, int>{std::error_code{}, 11}));
}

TEST(async_callback_test, basic_test__called_twice__void) {
    std::vector<std::error_code> invocations;
    {
        tested_async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        cb(std::error_code());
        cb(std::error_code());
        EXPECT_TRUE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(std::error_code()));
}

TEST(async_callback_test, basic_test__called_twice__param) {
    std::vector<std::pair<std::error_code, int>> invocations;
    {
        tested_async_callback<int> cb = [&](std::error_code ec, int arg) { invocations.emplace_back(ec, arg); };
        cb(std::error_code(), 10);
        cb(std::error_code(), 20);
        EXPECT_TRUE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(std::pair<std::error_code, int>{std::error_code(), 10}));
}

TEST(async_callback_test, empty_callback_does_nothing__void) {
    tested_async_callback<void> cb;
    EXPECT_FALSE(cb);
}

TEST(async_callback_test, empty_callback_does_nothing__param) {
    tested_async_callback<int> cb;
    EXPECT_FALSE(cb);
}

TEST(async_callback_test, move_constructor_test__void) {
    std::vector<std::error_code> invocations;
    {
        tested_async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        auto cb_copy = std::move(cb);
        EXPECT_FALSE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}

TEST(async_callback_test, move_constructor_test__param) {
    std::vector<std::pair<std::error_code, int>> invocations;
    {
        tested_async_callback<int> cb = [&](std::error_code ec, int arg) { invocations.emplace_back(ec, arg); };
        auto cb_copy = std::move(cb);
        EXPECT_FALSE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(std::pair<std::error_code, int>{
                                 make_error_code(errors::async_callback_err::not_called), int{}}));
}

TEST(async_callback_test, assignment_constructor__void) {
    std::vector<std::error_code> a_invocations;
    std::vector<std::error_code> b_invocations;
    {
        tested_async_callback<void> a_cb = [&](std::error_code ec) { a_invocations.emplace_back(ec); };
        tested_async_callback<void> b_cb = [&](std::error_code ec) { b_invocations.emplace_back(ec); };

        b_cb = std::move(a_cb);
        EXPECT_FALSE(a_cb);
    }

    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(a_invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
    EXPECT_THAT(b_invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}

TEST(async_callback_test, assignment_constructor__param) {
    std::vector<std::pair<std::error_code, int>> a_invocations;
    std::vector<std::pair<std::error_code, int>> b_invocations;
    {
        tested_async_callback<int> a_cb = [&](std::error_code ec, int arg) { a_invocations.emplace_back(ec, arg); };
        tested_async_callback<int> b_cb = [&](std::error_code ec, int arg) { b_invocations.emplace_back(ec, arg); };

        b_cb = std::move(a_cb);
        EXPECT_FALSE(a_cb);
    }

    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(a_invocations, ElementsAre(std::pair<std::error_code, int>{
                                   make_error_code(errors::async_callback_err::not_called), int{}}));
    EXPECT_THAT(b_invocations, ElementsAre(std::pair<std::error_code, int>{
                                   make_error_code(errors::async_callback_err::not_called), int{}}));
}

TEST(async_callback_test, null_assignment__void) {
    std::vector<std::error_code> invocations;
    {
        tested_async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        auto cb_copy = nullptr;
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}

TEST(async_callback_test, null_assignment__param) {
    std::vector<std::pair<std::error_code, int>> invocations;
    {
        tested_async_callback<int> cb = [&](std::error_code ec, int arg) { invocations.emplace_back(ec, arg); };
        auto cb_copy = nullptr;
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(std::pair<std::error_code, int>{
                                 make_error_code(errors::async_callback_err::not_called), int{}}));
}
