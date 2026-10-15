#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `ContainerOpenersCounter`.
namespace containerOpeners {

std::int32_t open(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z);

std::int32_t close(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::int32_t count(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z);

void clear();

} // namespace containerOpeners

} // namespace basalt::server::runtime::world
