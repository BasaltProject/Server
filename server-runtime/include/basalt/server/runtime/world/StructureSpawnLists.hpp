#pragma once

#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/world/structure/StructureQueries.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// parity: `NaturalSpawner.mobsAt` plus `ChunkGenerator.getMobsAt`. A start is only visible through
/// the position's own chunk, so an unloaded neighbour hides its structure.
class StructureSpawnLists {
public:
    explicit StructureSpawnLists(LoadedWorld& world);

    /// The override list for this position and category, or nothing to fall back to the biome.
    ///
    /// An override with no spawns answers an empty list, which is not the same as nothing: it means
    /// the structure silences the category.
    [[nodiscard]] std::optional<std::vector<SpawnerEntry>> listAt(std::string_view category, std::int32_t x,
                                                                  std::int32_t y, std::int32_t z) const;

private:
    /// parity: `NaturalSpawner.isInNetherFortressBounds`, a second and separate fortress gate that
    /// runs before the override table: standing on nether bricks anywhere in the fortress's full
    /// bounds gives the fortress list, even outside every piece.
    [[nodiscard]] bool isInNetherFortressBounds(std::string_view category,
                                                server::world::structure::StructurePos pos) const;

    LoadedWorld* world_;
    server::world::structure::StructureQueries queries_;
};

} // namespace basalt::server::runtime::world
