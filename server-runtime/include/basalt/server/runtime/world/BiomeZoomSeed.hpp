#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

[[nodiscard]] std::int64_t obfuscateBiomeSeed(std::int64_t seed);

} // namespace basalt::server::runtime::world
