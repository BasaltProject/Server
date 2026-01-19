#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/SelectableSlots.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::chiseledBookshelves {
namespace {

constexpr std::string_view kLastSlotKey = "last_interacted_slot";

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string occupiedKey(std::int32_t slot) { return "slot_" + std::to_string(slot) + "_occupied"; }

[[nodiscard]] server::world::ChunkBlockEntity* entityAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return nullptr;
    }
    if (server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15); existing != nullptr) {
        return existing;
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, block(), wire::nbt::NbtCompound{}});
    return chunk->blockEntityAt(x & 15, y, z & 15);
}

[[nodiscard]] std::optional<std::int32_t> occupy(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::int32_t stateId, std::int32_t slot, bool occupied) {
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
        block(), {{occupiedKey(slot), occupied ? "true" : "false"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return turned;
}

/// The shelf at these coordinates, if the slot is one it has and it is in the state wanted.
[[nodiscard]] bool shelfAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot,
                           bool occupied, std::int32_t& stateId) {
    const std::optional<std::int32_t> found = world.blockState(x, y, z);
    if (!found.has_value()) {
        return false;
    }
    stateId = *found;
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    if (id == nullptr || *id != block() || slot < 0 || slot >= kSize) {
        return false;
    }
    return (propertyOf(stateId, occupiedKey(slot)) == "true") == occupied;
}

[[nodiscard]] BlockEffect sound(const wire::common::Identifier& book, bool inserting) {
    const bool enchanted = book.path() == "enchanted_book";
    std::string name = inserting ? "block.chiseled_bookshelf.insert" : "block.chiseled_bookshelf.pickup";
    if (enchanted) {
        name += ".enchanted";
    }
    return blockEffect::Sound{
        .name = std::move(name), .source = wire::protocol::play::SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F};
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("chiseled_bookshelf");
    return instance;
}

bool isBookshelf(std::string_view path) { return path == block().path(); }

std::optional<std::int32_t> slotAt(std::int32_t stateId, std::string_view face, float clickX, float clickY,
                                   float clickZ) {
    const std::string facing = propertyOf(stateId, "facing");
    if (facing.empty()) {
        return std::nullopt;
    }
    return selectableSlots::hitSlot(facing, face, clickX, clickY, clickZ, kRows, kColumns);
}

std::optional<Interaction> putBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot,
                                   const wire::common::Identifier& book) {
    std::int32_t stateId = 0;
    if (!shelfAt(world, x, y, z, slot, false, stateId)) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned = occupy(world, x, y, z, stateId, slot, true);
    if (!turned.has_value()) {
        return std::nullopt;
    }
    blockInventory::Slots slots = blockInventory::read(entity->data(), kSize);
    slots[static_cast<std::size_t>(slot)] = server::world::loot::ItemStack{.item = book, .count = 1};
    blockInventory::write(entity->data(), slots);
    entity->data().putInt(kLastSlotKey, slot);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, x, y, z);
    return Interaction{.change =
                           BlockChange{.x = x, .y = y, .z = z, .stateId = *turned, .effects = {sound(book, true)}},
                       .taken = std::nullopt};
}

std::optional<Interaction> takeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t slot) {
    std::int32_t stateId = 0;
    if (!shelfAt(world, x, y, z, slot, true, stateId)) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned = occupy(world, x, y, z, stateId, slot, false);
    if (!turned.has_value()) {
        return std::nullopt;
    }
    blockInventory::Slots slots = blockInventory::read(entity->data(), kSize);
    const wire::common::Identifier book = slots[static_cast<std::size_t>(slot)].has_value()
                                              ? slots[static_cast<std::size_t>(slot)]->item
                                              : wire::common::Identifier::vanilla("book");
    slots[static_cast<std::size_t>(slot)] = std::nullopt;
    blockInventory::write(entity->data(), slots);
    entity->data().putInt(kLastSlotKey, slot);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, x, y, z);
    return Interaction{.change =
                           BlockChange{.x = x, .y = y, .z = z, .stateId = *turned, .effects = {sound(book, false)}},
                       .taken = book};
}

std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data) { return data.getInt(kLastSlotKey, -1) + 1; }

} // namespace basalt::server::runtime::world::chiseledBookshelves
