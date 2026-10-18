#include <basalt/server/runtime/command/DataArguments.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <algorithm>
#include <expected>
#include <random>
#include <string>

namespace basalt::server::runtime::world::containerLoot {

void fill(LoadedWorld& world, RealLootTables& tables, std::string_view table, std::int64_t seed,
          std::vector<std::optional<server::world::loot::ItemStack>>& slots, bool byPlayer,
          const std::array<std::int32_t, 3>& at, const std::optional<Opener>& opener) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(table);
    const server::world::loot::LootTable* rolled = id.has_value() ? tables.byId(*id) : nullptr;
    if (rolled == nullptr) {
        return;
    }
    server::world::loot::LootContext context{.hasEntity = byPlayer};
    // parity: `unpackLootTable` rolls with the player as `THIS_ENTITY` and with their luck.
    if (byPlayer && opener.has_value()) {
        context.luck = opener->luck;
        context.thisEntity = server::world::loot::EntityFacts{.type = wire::common::Identifier::vanilla("player"),
                                                              .mainhand = opener->mainhand,
                                                              .scoreName = opener->name};
    }
    // parity: `SlotProvider` over the `block_entity` being filled, read as its container slots.
    context.slotsOf = [&world, at](std::string_view source, std::string_view range) {
        std::vector<server::world::loot::ItemStack> found;
        server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
        if (source != "block_entity" || entity == nullptr) {
            return found;
        }
        const std::string path{entity->type().path()};
        const std::optional<std::int32_t> size = containers::sizeOf(path);
        const std::vector<command::SlotSourceValue>& ranges = command::slotRanges();
        const auto named = std::ranges::find(ranges, range, &command::SlotSourceValue::name);
        if (!size.has_value() || named == ranges.end()) {
            return found;
        }
        const blockInventory::Slots held = blockInventory::readFor(path, entity->data(), *size);
        for (const std::int32_t id : named->slots) {
            if (id >= 0 && id < *size && held[static_cast<std::size_t>(id)].has_value()) {
                found.push_back(*held[static_cast<std::size_t>(id)]);
            }
        }
        return found;
    };
    // parity: `Vec3.atCenterOf(worldPosition)`.
    tables.prepareAt(context, world, at[0] + 0.5, at[1] + 0.5, at[2] + 0.5);
    // parity: `withOptionalRandomSeed`.
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

std::optional<wire::common::Identifier> unpack(LoadedWorld& world, RealLootTables& tables, std::int32_t x,
                                               std::int32_t y, std::int32_t z,
                                               const std::optional<wire::common::Uuid>& player,
                                               const std::optional<Opener>& opener) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::string table{entity->data().getString(kTableKey)};
    if (table.empty()) {
        return std::nullopt;
    }
    const std::string path{entity->type().path()};
    const std::optional<std::int32_t> size = containers::sizeOf(path);
    const std::int64_t seed = entity->data().getLong(kSeedKey);
    // parity: `setLootTable(null)` goes first.
    static_cast<void>(entity->data().remove(kTableKey));
    static_cast<void>(entity->data().remove(kSeedKey));
    chunk->markDirty();
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> rolled =
        wire::common::Identifier::parse(table);
    const std::optional<wire::common::Identifier> named = rolled.has_value() ? std::optional{*rolled} : std::nullopt;
    if (!size.has_value()) {
        return named;
    }
    blockInventory::Slots slots = blockInventory::readFor(path, entity->data(), *size);
    fill(world, tables, table, seed, slots, player.has_value(), {x, y, z}, opener);
    // The roll may have written blocks; look the entity up again rather than trust the pointer.
    server::world::ChunkBlockEntity* again = chunk->blockEntityAt(x & 15, y, z & 15);
    if (again != nullptr) {
        blockInventory::writeFor(path, again->data(), slots);
    }
    return named;
}

} // namespace basalt::server::runtime::world::containerLoot
