#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime::world {

/// A player's wait before feeding the same kind of animal into love again; off at 0 seconds.
namespace breedingCooldowns {

[[nodiscard]] bool active(const std::string& world, const std::string& player, const wire::common::Identifier& type,
                          std::int64_t now);

void record(const std::string& world, const std::string& player, const wire::common::Identifier& type, std::int64_t now,
            std::int32_t seconds);

} // namespace breedingCooldowns

} // namespace basalt::server::runtime::world
