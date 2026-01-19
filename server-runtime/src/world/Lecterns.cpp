#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::lecterns {
namespace {

constexpr std::string_view kBookKey = "Book";
constexpr std::string_view kPageKey = "Page";
constexpr std::string_view kPagesKey = "Pages";

/// parity: `LecternBlockEntity.getRedstoneSignal`'s fourteen steps between the covers.
constexpr float kSignalSteps = 14.0F;

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

/// The lectern at these coordinates, and the block entity it keeps its book in.
[[nodiscard]] server::world::ChunkBlockEntity* entityAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z, bool create) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return nullptr;
    }
    if (server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15); existing != nullptr) {
        return existing;
    }
    if (!create) {
        return nullptr;
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, block(), wire::nbt::NbtCompound{}});
    return chunk->blockEntityAt(x & 15, y, z & 15);
}

/// Answers the lectern's own block, or nothing when this is not one or does not say what is wanted.
[[nodiscard]] const wire::common::Identifier* lecternAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z, std::int32_t& stateId,
                                                        const std::string& property, const std::string& wanted) {
    const std::optional<std::int32_t> found = world.blockState(x, y, z);
    if (!found.has_value()) {
        return nullptr;
    }
    stateId = *found;
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    if (id == nullptr || *id != block() || propertyOf(stateId, property) != wanted) {
        return nullptr;
    }
    return id;
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("lectern");
    return instance;
}

bool isBook(const wire::common::Identifier& item) {
    return item.path() == "written_book" || item.path() == "writable_book";
}

std::optional<BlockChange> placeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const wire::common::Identifier& book, std::int32_t pages) {
    std::int32_t stateId = 0;
    const wire::common::Identifier* id = lecternAt(world, x, y, z, stateId, "has_book", "false");
    if (id == nullptr) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, true);
    if (entity == nullptr) {
        return std::nullopt;
    }
    // parity: `resetBookState`, which clears the pulse as it puts the book down.
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*id, {{"has_book", "true"}, {"powered", "false"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    entity->data().putString(kBookKey, book.toString());
    entity->data().putInt(kPageKey, 0);
    entity->data().putInt(kPagesKey, std::max(1, pages));
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, x, y, z);
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

std::optional<std::pair<BlockChange, wire::common::Identifier>> takeBook(WorldState& world, std::int32_t x,
                                                                         std::int32_t y, std::int32_t z) {
    std::int32_t stateId = 0;
    const wire::common::Identifier* id = lecternAt(world, x, y, z, stateId, "has_book", "true");
    if (id == nullptr) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> book =
        wire::common::Identifier::parse(std::string{entity->data().getString(kBookKey)});
    if (!book.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*id, {{"has_book", "false"}, {"powered", "false"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    entity->data().remove(kBookKey);
    entity->data().remove(kPageKey);
    entity->data().remove(kPagesKey);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, x, y, z);
    return std::pair{BlockChange{.x = x, .y = y, .z = z, .stateId = *turned}, *book};
}

std::optional<BlockChange> turnPage(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t page) {
    std::int32_t stateId = 0;
    const wire::common::Identifier* id = lecternAt(world, x, y, z, stateId, "has_book", "true");
    if (id == nullptr) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::int32_t pages = entity->data().getInt(kPagesKey, kDefaultPages);
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*id, {{"powered", "true"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    entity->data().putInt(kPageKey, std::clamp(page, 0, std::max(0, pages - 1)));
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, x, y, z);
    world.scheduleBlockTick(x, y, z, block(), kPulseTicks);
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

std::optional<BlockChange> endPulse(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t stateId = 0;
    const wire::common::Identifier* id = lecternAt(world, x, y, z, stateId, "powered", "true");
    if (id == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*id, {{"powered", "false"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data) {
    const bool hasBook = !data.getString(kBookKey).empty();
    const std::int32_t pages = data.getInt(kPagesKey, kDefaultPages);
    const std::int32_t page = data.getInt(kPageKey, 0);
    const float progress = pages > 1 ? static_cast<float>(page) / static_cast<float>(pages - 1) : 1.0F;
    return static_cast<std::int32_t>(std::floor(progress * kSignalSteps)) + (hasBook ? 1 : 0);
}

} // namespace basalt::server::runtime::world::lecterns
