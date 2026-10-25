#include <basalt/server/runtime/command/DataArguments.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <algorithm>
#include <chrono>
#include <expected>
#include <random>
#include <span>
#include <string>

namespace basalt::server::runtime::world::containerLoot {
namespace {

constexpr std::string_view kLootableKey = "Paper.LootableData";

[[nodiscard]] std::int64_t nowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

[[nodiscard]] std::vector<std::int32_t> intsOf(const wire::common::Uuid& id) {
    return {static_cast<std::int32_t>(id.high() >> 32), static_cast<std::int32_t>(id.high()),
            static_cast<std::int32_t>(id.low() >> 32), static_cast<std::int32_t>(id.low())};
}

[[nodiscard]] std::optional<wire::common::Uuid> uuidOf(std::span<const std::int32_t> ints) {
    if (ints.size() != 4) {
        return std::nullopt;
    }
    const auto part = [](std::int32_t value) { return static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)); };
    return wire::common::Uuid{(part(ints[0]) << 32) | part(ints[1]), (part(ints[2]) << 32) | part(ints[3])};
}

} // namespace

Lootable Lootable::read(const wire::nbt::NbtCompound& data) {
    Lootable found;
    const wire::nbt::NbtCompound* root = data.getCompound(kLootableKey);
    if (root == nullptr) {
        return found;
    }
    found.lastFill = root->getLong("lastFill", -1);
    found.nextRefill = root->getLong("nextRefill", -1);
    found.numRefills = root->getInt("numRefills", 0);
    if (const wire::nbt::NbtList* list = root->getList("lootedPlayers"); list != nullptr) {
        for (const wire::nbt::NbtTag& tag : *list) {
            const auto* entry = tag.get<wire::nbt::NbtCompound>();
            const std::optional<wire::common::Uuid> id =
                entry == nullptr ? std::nullopt : uuidOf(entry->getIntArray("UUID"));
            if (id.has_value()) {
                found.looted.emplace_back(*id, entry->getLong("Time", 0));
            }
        }
    }
    return found;
}

void Lootable::write(wire::nbt::NbtCompound& data) const {
    wire::nbt::NbtCompound root;
    if (nextRefill != -1) {
        root.putLong("nextRefill", nextRefill);
    }
    if (lastFill != -1) {
        root.putLong("lastFill", lastFill);
    }
    if (numRefills != 0) {
        root.putInt("numRefills", numRefills);
    }
    if (!looted.empty()) {
        wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
        for (const auto& [id, time] : looted) {
            wire::nbt::NbtCompound entry;
            entry.putIntArray("UUID", intsOf(id));
            entry.putLong("Time", time);
            list.push(wire::nbt::NbtTag::ofCompound(std::move(entry)));
        }
        root.set("lootedPlayers", wire::nbt::NbtTag::ofList(std::move(list)));
    }
    if (root.empty()) {
        static_cast<void>(data.remove(kLootableKey));
        return;
    }
    data.set(kLootableKey, wire::nbt::NbtTag::ofCompound(std::move(root)));
}

bool Lootable::canLoot(const wire::common::Uuid& player, const std::int64_t now,
                       const std::int32_t relootSeconds) const {
    const auto found = std::ranges::find(looted, player, &std::pair<wire::common::Uuid, std::int64_t>::first);
    if (found == looted.end()) {
        return true;
    }
    return relootSeconds >= 0 && found->second + (static_cast<std::int64_t>(relootSeconds) * 1000) < now;
}

bool Lootable::shouldRefill(const std::optional<wire::common::Uuid>& player, const std::int64_t now, const bool refill,
                            const std::int32_t relootSeconds, const std::int32_t maxRefills) const {
    if (lastFill == -1 || !refill) {
        return true;
    }
    return player.has_value() && nextRefill != -1 && (maxRefills == -1 || numRefills < maxRefills) &&
           nextRefill <= now && canLoot(*player, now, relootSeconds);
}

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
    const config::WorldSettings& settings = world.settings();
    const std::int64_t now = nowMillis();
    const bool refill = world.tunables().lootRefill;
    Lootable lootable = Lootable::read(entity->data());
    if (!lootable.shouldRefill(player, now, refill, world.tunables().lootRelootSeconds, settings.lootMaxRefills)) {
        return std::nullopt;
    }
    const std::string path{entity->type().path()};
    const std::optional<std::int32_t> size = containers::sizeOf(path);
    const std::int64_t seed = entity->data().getLong(kSeedKey);
    lootable.lastFill = now;
    if (refill) {
        // parity: Paper's `shouldClearLootTable` keeps the table, the seed resets and the next refill is drawn.
        const std::int64_t span =
            static_cast<std::int64_t>(settings.lootRefillMaxSeconds) - settings.lootRefillMinSeconds + 1;
        std::mt19937_64 engine{std::random_device{}()};
        const std::int64_t wait =
            settings.lootRefillMinSeconds + static_cast<std::int64_t>(engine() % static_cast<std::uint64_t>(span));
        lootable.nextRefill = now + (wait * 1000);
        ++lootable.numRefills;
        entity->data().putLong(kSeedKey, 0);
        if (player.has_value()) {
            std::erase_if(lootable.looted, [&player](const auto& one) { return one.first == *player; });
            lootable.looted.emplace_back(*player, now);
        }
    } else {
        // parity: `setLootTable(null)` goes first.
        static_cast<void>(entity->data().remove(kTableKey));
        static_cast<void>(entity->data().remove(kSeedKey));
    }
    lootable.write(entity->data());
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
