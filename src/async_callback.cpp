#include "async_callback.hpp"

namespace lsem::async_kit {

namespace errors {
namespace {
struct async_callback_err_cat : std::error_category {
   public:
    const char* name() const noexcept override { return "async_callback"; }
    std::string message(int ev) const override {
        switch (static_cast<async_callback_err>(ev)) {
            case async_callback_err::not_called:
                return "not_called";
            default:
                return "unrecognized error";
        }
    }
};

const async_callback_err_cat the_async_callback_category;
}  // namespace

std::error_code make_error_code(async_callback_err e) {
    return {static_cast<int>(e), the_async_callback_category};
}

}  // namespace errors

}  // namespace lsem::async_kit