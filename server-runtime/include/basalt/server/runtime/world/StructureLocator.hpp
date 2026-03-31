#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// Passes over a start chunk somebody already claimed, for an explorer map.
using ClaimedStart = std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>;

/// A structure `/locate` and an explorer map can find, and the search that finds it: the start chunk
/// vanilla's walk of the placement grid reaches first from the given chunk within `maxCells` rings,
/// or nothing.
struct StructureLocator {
    wire::common::Identifier id;
    std::function<std::optional<std::pair<std::int32_t, std::int32_t>>(
        std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, std::int32_t maxCells, const ClaimedStart& skip)>
        locate;
};

} // namespace basalt::server::runtime::world
