#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <expected>
#include <random>
#include <string>

namespace basalt::server::runtime::world::containerLoot {

void fill(LoadedWorld& world, RealLootTables& tables, std::string_view table, std::int64_t seed,
          std::vector<std::optional<server::world::loot::ItemStack>>& slots, bool byPlayer,
          const std::array<std::int32_t, 3>& at) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(table);
    const server::world::loot::LootTable* rolled = id.has_value() ? tables.byId(*id) : nullptr;
    if (rolled == nullptr) {
        return;
    }
    server::world::loot::LootContext context{.hasEntity = byPlayer};
    // parity: `Vec3.atCenterOf(worldPosition)`.
    tables.prepareAt(context, world, at[0] + 0.5, at[1] + 0.5, at[2] + 0.5);
    // parity: `withOptionalRandomSeed`, a fresh `LegacyRandomSource` unless the seed is zero, and
    // then the table's own sequence, or the level's random when it names none.
    if (seed != 0) {
        random::RandomSource random = random::RandomSource::legacy(seed);
        rolled->fill(slots, random, context);
        return;
    }
    if (rolled->randomSequence.has_value()) {
        rolled->fill(slots, world.randomSequences().of(*rolled->randomSequence), context);
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
    rolled->fill(slots, random, context);
}

void unpack(LoadedWorld& world, RealLootTables& tables, std::int32_t x, std::int32_t y, std::int32_t z,
            const std::optional<wire::common::Uuid>& player) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr) {
        return;
    }
    const std::string table{entity->data().getString(kTableKey)};
    if (table.empty()) {
        return;
    }
    const std::string path{entity->type().path()};
    const std::optional<std::int32_t> size = containers::sizeOf(path);
    const std::int64_t seed = entity->data().getLong(kSeedKey);
    // parity: `setLootTable(null)` goes first, so a roll that reads the container finds no table.
    static_cast<void>(entity->data().remove(kTableKey));
    static_cast<void>(entity->data().remove(kSeedKey));
    chunk->markDirty();
    if (!size.has_value()) {
        return;
    }
    blockInventory::Slots slots = blockInventory::readFor(path, entity->data(), *size);
    fill(world, tables, table, seed, slots, player.has_value(), {x, y, z});
    // The roll may have written blocks; look the entity up again rather than trust the pointer.
    server::world::ChunkBlockEntity* again = chunk->blockEntityAt(x & 15, y, z & 15);
    if (again != nullptr) {
        blockInventory::writeFor(path, again->data(), slots);
    }
}

} // namespace basalt::server::runtime::world::containerLoot
