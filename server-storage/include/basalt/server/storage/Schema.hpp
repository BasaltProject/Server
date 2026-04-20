#pragma once

#include <string_view>

namespace basalt::server::storage {
namespace schema {
[[nodiscard]] std::string_view ddl();
} // namespace schema
} // namespace basalt::server::storage
