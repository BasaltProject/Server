#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::structure {

/// parity: the `spawn_overrides` field of the five vanilla structures that carry one. An empty spawn
/// list is meaningful: it silences a whole category inside the structure.
namespace structureSpawnOverrides {

/// Which box a category's override is tested against. parity: `BoundingBoxType`.
enum class Bounds : std::uint8_t { Piece, Full };

struct SpawnEntry {
    std::string type;
    std::int32_t weight = 0;
    std::int32_t minCount = 0;
    std::int32_t maxCount = 0;

    friend bool operator==(const SpawnEntry&, const SpawnEntry&) = default;
};

struct Override {
    Bounds bounds = Bounds::Full;
    std::vector<SpawnEntry> spawns;

    friend bool operator==(const Override&, const Override&) = default;
};

/// The structures that override anything, so a caller can skip the rest of the index.
[[nodiscard]] const std::set<std::string, std::less<>>& structures();

[[nodiscard]] const Override* of(std::string_view structure, std::string_view category);

} // namespace structureSpawnOverrides

} // namespace basalt::server::world::structure
