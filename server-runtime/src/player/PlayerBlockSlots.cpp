#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockSlots.hpp>
#include <basalt/server/runtime/player/PlayerFaces.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

using wire::protocol::play::Gamemode;

[[nodiscard]] std::optional<wire::common::Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = wire::common::Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// One off the stack in hand, and the whole screen redrawn where a slot is not enough.
void spendOne(const BreakContext& context, PlayerSession& player, const std::int32_t heldSlot,
              const storage::StoredItemStack& held) {
    if (player.gamemode == Gamemode::Creative) {
        return;
    }
    storage::StoredItemStack left = held;
    left.count = held.count - 1;
    player.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{left});
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
}

/// Into the bag if it fits, on the ground if it does not.
void giveOrDrop(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                const storage::StoredItemStack& stack, const std::int32_t x, const std::int32_t y,
                const std::int32_t z) {
    const std::int32_t leftover = player.inventory.add(stack);
    if (player.connection().sendInventoryAll) {
        player.connection().sendInventoryAll();
    }
    const std::optional<wire::common::Identifier> item = parsedOrNothing(stack.id);
    if (leftover <= 0 || !item.has_value()) {
        return;
    }
    spawnDroppedItem(context, world, *item, leftover, x + 0.5, y + 0.5, z + 0.5, 0.0, 0.0, 0.0, 0,
                     wire::nbt::NbtCompound{});
}

} // namespace

void useBookshelfFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                     const std::int32_t x, const std::int32_t y, const std::int32_t z, const FaceHit& hit,
                     const std::int32_t heldSlot, const std::optional<storage::StoredItemStack>& held,
                     const world::realItemTags::TagMembers* itemTags) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::optional<std::int32_t> slot =
        world::chiseledBookshelves::slotAt(*stateId, faceName(hit.face), hit.x, hit.y, hit.z);
    if (!slot.has_value()) {
        return;
    }
    const std::optional<wire::common::Identifier> book =
        held.has_value() ? parsedOrNothing(held->id) : std::optional<wire::common::Identifier>{};
    const bool isBook = book.has_value() && itemTags != nullptr &&
                        world::realItemTags::contains(*itemTags, "minecraft:bookshelf_books", *book);
    const std::optional<world::chiseledBookshelves::Interaction> interaction =
        isBook ? world::chiseledBookshelves::putBook(world.state(), x, y, z, *slot, *book)
               : world::chiseledBookshelves::takeBook(world.state(), x, y, z, *slot);
    if (!interaction.has_value()) {
        return;
    }
    announceBlock(context, world.name(), x, y, z, interaction->change.stateId);
    if (context.blocks != nullptr) {
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, interaction->change.stateId,
                                    interaction->change.effects);
    }
    if (interaction->taken.has_value()) {
        giveOrDrop(context, player, world, storage::StoredItemStack{.id = interaction->taken->toString(), .count = 1},
                   x, y, z);
        return;
    }
    if (held.has_value()) {
        spendOne(context, player, heldSlot, *held);
    }
}

void usePotFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
               const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
               const std::optional<storage::StoredItemStack>& held) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (!stateId.has_value() || chunk == nullptr || context.blocks == nullptr) {
        return;
    }
    server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    const std::optional<wire::common::Identifier> item =
        held.has_value() ? parsedOrNothing(held->id) : std::optional<wire::common::Identifier>{};
    std::optional<std::vector<world::BlockEffect>> inserted;
    if (entity != nullptr && item.has_value()) {
        inserted = world::decoratedPots::insert(entity->data(), *item);
    }
    // An empty hand, a full pot or the wrong item all end the same way: the pot rocks and takes
    // nothing.
    if (!inserted.has_value()) {
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, *stateId,
                                    world::decoratedPots::rebuff());
        return;
    }
    chunk->markDirty();
    world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z,
                                                         world::comparatorReading::of(world.state()));
    spendOne(context, player, heldSlot, *held);
    context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, *stateId, *inserted);
}

} // namespace basalt::server::runtime::player
