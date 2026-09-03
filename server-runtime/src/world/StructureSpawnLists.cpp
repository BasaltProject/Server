#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/StructureSpawnLists.hpp>
#include <basalt/server/world/structure/StructureSpawnOverrides.hpp>

namespace basalt::server::runtime::world {
namespace {

namespace structure = server::world::structure;

using wire::common::Identifier;

[[nodiscard]] std::vector<SpawnerEntry> entriesOf(const structure::structureSpawnOverrides::Override& override) {
    std::vector<SpawnerEntry> entries;
    entries.reserve(override.spawns.size());
    for (const structure::structureSpawnOverrides::SpawnEntry& entry : override.spawns) {
        const std::expected<Identifier, wire::common::IdentifierError> type = Identifier::parse(entry.type);
        if (type.has_value()) {
            entries.push_back(SpawnerEntry{
                .type = *type, .weight = entry.weight, .minCount = entry.minCount, .maxCount = entry.maxCount});
        }
    }
    return entries;
}

[[nodiscard]] const std::vector<SpawnerEntry>& fortressMonsters() {
    static const std::vector<SpawnerEntry> all = [] {
        const structure::structureSpawnOverrides::Override* override =
            structure::structureSpawnOverrides::of("fortress", "monster");
        return override == nullptr ? std::vector<SpawnerEntry>{} : entriesOf(*override);
    }();
    return all;
}

} // namespace

StructureSpawnLists::StructureSpawnLists(LoadedWorld& world)
    : world_(&world), queries_([&world](std::int32_t chunkX, std::int32_t chunkZ) {
          const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
          return chunk == nullptr ? nullptr : &chunk->structures();
      }) {}

std::optional<std::vector<SpawnerEntry>> StructureSpawnLists::listAt(std::string_view category, std::int32_t x,
                                                                     std::int32_t y, std::int32_t z) const {
    const structure::StructurePos pos{x, y, z};
    if (isInNetherFortressBounds(category, pos)) {
        return fortressMonsters();
    }
    for (const structure::StructureStartRecord& start : queries_.startsIn(pos)) {
        const structure::structureSpawnOverrides::Override* override =
            structure::structureSpawnOverrides::of(start.structure, category);
        if (override == nullptr) {
            continue;
        }
        const bool inside = override->bounds == structure::structureSpawnOverrides::Bounds::Piece
                                ? start.hasPieceAt(pos)
                                : start.contains(pos);
        if (inside) {
            return entriesOf(*override);
        }
    }
    return std::nullopt;
}

bool StructureSpawnLists::isInNetherFortressBounds(std::string_view category, structure::StructurePos pos) const {
    if (category != "monster") {
        return false;
    }
    const server::world::Chunk* below = world_->state().chunkIfLoaded(pos.x >> 4, pos.z >> 4);
    if (below == nullptr) {
        return false;
    }
    const std::int32_t state = below->blockStateId(pos.x & 15, pos.y - 1, pos.z & 15);
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(state);
    if (!block.has_value() || block->block.path() != "nether_bricks") {
        return false;
    }
    return queries_.startAt(pos, "fortress") != nullptr;
}

} // namespace basalt::server::runtime::world
