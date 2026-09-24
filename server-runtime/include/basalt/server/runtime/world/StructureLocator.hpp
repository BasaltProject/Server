#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// A structure `/locate` can find, and the search that finds it: the start chunk vanilla's walk of
/// the placement grid reaches first from the given chunk, or nothing within its reach.
struct StructureLocator {
    wire::common::Identifier id;
    std::function<std::optional<std::pair<std::int32_t, std::int32_t>>(std::int64_t seed, std::int32_t chunkX,
                                                                       std::int32_t chunkZ)>
        locate;
};

} // namespace basalt::server::runtime::world
