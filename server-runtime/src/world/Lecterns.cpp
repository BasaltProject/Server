#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::lecterns {
namespace {

/// parity: `LecternBlock.updateBelow`. Every path that writes a lectern ends in it. The lectern's own
/// `setBlock` has already told this square's neighbours; what this adds is the square below and its
/// own, which is where a line of dust running under the stand reads from. A lectern written by
/// command gets none of it, because a command never goes through these paths.
void tellBelow(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    redstoneComponents::scheduleAround(world, {}, x, y - 1, z, false, comparatorReading::of(world));
}

constexpr std::string_view kBookKey = "Book";
constexpr std::string_view kPageKey = "Page";

/// parity: `LecternBlockEntity.getPageCount`, which asks the book itself: a written one keeps its
/// pages under `written_book_content` and a plain one under `writable_book_content`, and a book with
/// neither has no pages at all.
constexpr std::string_view kWrittenContent = "minecraft:written_book_content";
constexpr std::string_view kWritableContent = "minecraft:writable_book_content";
constexpr std::string_view kPagesField = "pages";
constexpr std::string_view kResolvedField = "resolved";

/// parity: `LecternBlockEntity.getRedstoneSignal`'s fourteen steps between the covers.
constexpr float kSignalSteps = 14.0F;

/// The book on the stand, or nothing when there is none.
[[nodiscard]] std::optional<server::world::loot::ItemStack> bookIn(const wire::nbt::NbtCompound& data) {
    const wire::nbt::NbtCompound* stored = data.getCompound(std::string{kBookKey});
    return stored == nullptr ? std::nullopt : blockInventory::readStack(*stored);
}

[[nodiscard]] std::int32_t pageCountOf(const wire::nbt::NbtCompound& data) {
    const std::optional<server::world::loot::ItemStack> book = bookIn(data);
    if (!book.has_value()) {
        return 0;
    }
    for (const std::string_view key : {kWrittenContent, kWritableContent}) {
        const wire::nbt::NbtCompound* content = book->components.getCompound(std::string{key});
        const wire::nbt::NbtList* pages = content == nullptr ? nullptr : content->getList(std::string{kPagesField});
        if (pages != nullptr) {
            return static_cast<std::int32_t>(pages->size());
        }
    }
    return 0;
}

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
                                     const server::world::loot::ItemStack& book) {
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
    // parity: `saveAdditional` writes the book with `ItemStack.CODEC` and the page beside it, and
    // writes neither while the stand is empty.
    entity->data().set(std::string{kBookKey}, wire::nbt::NbtTag::ofCompound(blockInventory::writeStack(book)));
    entity->data().putInt(kPageKey, 0);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    // parity: `setBook` resolves what it is handed before it keeps it.
    static_cast<void>(resolveBook(world, x, y, z));
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    tellBelow(world, x, y, z);
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

bool resolveBook(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr) {
        return false;
    }
    const auto inside = [](wire::nbt::NbtCompound* holder, std::string_view key) -> wire::nbt::NbtCompound* {
        wire::nbt::NbtTag* tag = holder == nullptr ? nullptr : holder->find(key);
        return tag == nullptr ? nullptr : tag->get<wire::nbt::NbtCompound>();
    };
    wire::nbt::NbtCompound* book = inside(&entity->data(), kBookKey);
    wire::nbt::NbtCompound* content = inside(inside(book, "components"), kWrittenContent);
    // parity: `WrittenBookContent.resolve` answers nothing when it is already resolved, and the codec
    // leaves the field out while it is false.
    if (content == nullptr || content->getBoolean(std::string{kResolvedField})) {
        return false;
    }
    content->putBoolean(std::string{kResolvedField}, true);
    if (server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4); chunk != nullptr) {
        chunk->markDirty();
    }
    return true;
}

std::optional<std::pair<BlockChange, server::world::loot::ItemStack>> takeBook(WorldState& world, std::int32_t x,
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
    const std::optional<server::world::loot::ItemStack> book = bookIn(entity->data());
    if (!book.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*id, {{"has_book", "false"}, {"powered", "false"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    static_cast<void>(entity->data().remove(std::string{kBookKey}));
    static_cast<void>(entity->data().remove(std::string{kPageKey}));
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    tellBelow(world, x, y, z);
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
    const std::int32_t pages = pageCountOf(entity->data());
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*id, {{"powered", "true"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    entity->data().putInt(kPageKey, std::clamp(page, 0, std::max(0, pages - 1)));
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    world.scheduleBlockTick(x, y, z, block(), kPulseTicks);
    tellBelow(world, x, y, z);
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
    tellBelow(world, x, y, z);
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data) {
    const bool hasBook = bookIn(data).has_value();
    const std::int32_t pages = pageCountOf(data);
    const std::int32_t page = data.getInt(kPageKey, 0);
    const float progress = pages > 1 ? static_cast<float>(page) / static_cast<float>(pages - 1) : 1.0F;
    return static_cast<std::int32_t>(std::floor(progress * kSignalSteps)) + (hasBook ? 1 : 0);
}

} // namespace basalt::server::runtime::world::lecterns
