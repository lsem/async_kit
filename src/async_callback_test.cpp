#include "async_callback.hpp"

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace errors = lsem::async_kit::errors;
using lsem::async_kit::async_callback;

using testing::ElementsAre;

TEST(async_callback_test, basic_test__not_called) {
    std::vector<std::error_code> invocations;
    {
        async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        EXPECT_TRUE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}

TEST(async_callback_test, basic_test__called_twice) {
    std::vector<std::error_code> invocations;
    {
        async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        cb(std::error_code());
        cb(std::error_code());
        EXPECT_TRUE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(std::error_code()));
}

TEST(async_callback_test, empty_callback_does_nothing) {
    async_callback<void> cb;
    EXPECT_FALSE(cb);
}

TEST(async_callback_test, move_constructor_test) {
    std::vector<std::error_code> invocations;
    {
        async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        auto cb_copy = std::move(cb);
        EXPECT_FALSE(cb);
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}

TEST(async_callback_test, assignment_constructor) {
    std::vector<std::error_code> a_invocations;
    std::vector<std::error_code> b_invocations;
    {
        async_callback<void> a_cb = [&](std::error_code ec) { a_invocations.emplace_back(ec); };
        async_callback<void> b_cb = [&](std::error_code ec) { b_invocations.emplace_back(ec); };

        b_cb = std::move(a_cb);
        EXPECT_FALSE(b_cb);
    }

    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(a_invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
    EXPECT_THAT(b_invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}

TEST(async_callback_test, null_assignment) {
    std::vector<std::error_code> invocations;
    {
        async_callback<void> cb = [&](std::error_code ec) { invocations.emplace_back(ec); };
        auto cb_copy = nullptr;
    }
    // callback gets auto called during destruction if not called manually.
    EXPECT_THAT(invocations, ElementsAre(make_error_code(errors::async_callback_err::not_called)));
}
