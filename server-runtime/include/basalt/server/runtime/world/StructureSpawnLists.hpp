#pragma once

#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/world/structure/StructureQueries.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// parity: `NaturalSpawner.mobsAt`.
class StructureSpawnLists {
public:
    explicit StructureSpawnLists(LoadedWorld& world);

    /// An override with no spawns is an empty list, not nothing: the structure silences the category.
    [[nodiscard]] std::optional<std::vector<SpawnerEntry>> listAt(std::string_view category, std::int32_t x,
                                                                  std::int32_t y, std::int32_t z) const;

private:
    /// parity: `NaturalSpawner.isInNetherFortressBounds`.
    [[nodiscard]] bool isInNetherFortressBounds(std::string_view category,
                                                server::world::structure::StructurePos pos) const;

    LoadedWorld* world_;
    server::world::structure::StructureQueries queries_;
};

} // namespace basalt::server::runtime::world
