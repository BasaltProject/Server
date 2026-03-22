#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/StructureSpawnLists.hpp>
#include <basalt/server/world/structure/StructureSpawnOverrides.hpp>

#include <cstdint>

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
            entries.push_back(SpawnerEntry{.type = *type,
                                           .weight = entry.weight,
                                           .minCount = entry.minCount,
                                           .maxCount = entry.maxCount,
                                           .constantCount = entry.constantCount});
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
    // Through the world and not the chunk: a spawn candidate sits as low as the world's floor, so
    // the block under it is below the bedrock. The world answers that with nothing, which is the
    // no this wants; the chunk throws, and the whole spawning turn died with it on a void world.
    const std::optional<std::int32_t> state = world_->state().blockState(pos.x, pos.y - 1, pos.z);
    if (!state.has_value()) {
        return false;
    }
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(*state);
    if (!block.has_value() || block->block.path() != "nether_bricks") {
        return false;
    }
    return queries_.startAt(pos, "fortress") != nullptr;
}

} // namespace basalt::server::runtime::world
