#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockSlots.hpp>
#include <basalt/server/runtime/player/PlayerFaces.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
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
        inserted = world::decoratedPots::insert(entity->data(), *item, held->components);
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

namespace {

/// parity: `ShelfBlock.swapHotbar`, which trades the player's nine for the shelf's own.
void swapHotbarWithShelf(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                         const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t stateId) {
    world::blockInventory::Slots hotbar(PlayerInventory::kHotbarSize);
    for (std::int32_t index = 0; index < PlayerInventory::kHotbarSize; index++) {
        const std::optional<storage::StoredItemStack>& stack =
            player.inventory.at(PlayerInventory::kHotbarStart + index);
        const std::optional<wire::common::Identifier> item =
            stack.has_value() ? parsedOrNothing(stack->id) : std::optional<wire::common::Identifier>{};
        if (item.has_value()) {
            hotbar[static_cast<std::size_t>(index)] =
                server::world::loot::ItemStack{.item = *item, .count = stack->count};
        }
    }
    if (!world::shelves::swapHotbar(world.state(), x, y, z, hotbar)) {
        return;
    }
    for (std::int32_t index = 0; index < PlayerInventory::kHotbarSize; index++) {
        const std::optional<server::world::loot::ItemStack>& stack = hotbar[static_cast<std::size_t>(index)];
        player.inventory.set(PlayerInventory::kHotbarStart + index,
                             stack.has_value() ? std::optional{storage::StoredItemStack{.id = stack->item.toString(),
                                                                                        .count = stack->count}}
                                               : std::nullopt);
    }
    if (player.connection().sendInventoryAll) {
        player.connection().sendInventoryAll();
    }
    // parity: `swapHotbar` calls `setChanged(ENTITY_INTERACT)` on every shelf of the chain, and each
    // one is a noise of its own.
    for (const world::shelves::Position& part : world::shelves::chainAt(world.state(), x, y, z)) {
        if (world.state().hasSculkSensorNear(part[0], part[1], part[2], world::vibrations::kCalibratedRange)) {
            world::vibrations::emit(world.state(), "entity_interact", part[0] + 0.5, part[1] + 0.5, part[2] + 0.5,
                                    world.gameTime(), {.entity = player.uuid()});
        }
    }
    if (context.blocks != nullptr) {
        const std::vector<world::BlockEffect> noise{
            world::blockEffect::Sound{.name = "block.shelf.multi_swap",
                                      .source = wire::protocol::play::SoundSource::Blocks,
                                      .volume = 1.0F,
                                      .pitch = 1.0F}};
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, stateId, noise);
    }
}

} // namespace

void useShelfFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
                 const std::int32_t y, const std::int32_t z, const FaceHit& hit, const std::int32_t heldSlot,
                 const std::optional<storage::StoredItemStack>& held) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::optional<std::int32_t> slot = world::shelves::slotAt(*stateId, faceName(hit.face), hit.x, hit.y, hit.z);
    if (!slot.has_value()) {
        return;
    }
    if (server::registry::blockStateDefinitions::propertiesOf(*stateId)["powered"] == "true") {
        swapHotbarWithShelf(context, player, world, x, y, z, *stateId);
        return;
    }
    std::optional<server::world::loot::ItemStack> placed;
    if (held.has_value()) {
        if (const std::optional<wire::common::Identifier> item = parsedOrNothing(held->id); item.has_value()) {
            placed = server::world::loot::ItemStack{.item = *item, .count = held->count};
        }
    }
    const std::optional<world::shelves::SwapResult> result =
        world::shelves::swap(world.state(), x, y, z, *slot, placed);
    if (!result.has_value()) {
        return;
    }
    player.inventory.set(heldSlot, result->removed.has_value()
                                       ? std::optional{storage::StoredItemStack{.id = result->removed->item.toString(),
                                                                                .count = result->removed->count}}
                                       : std::nullopt);
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
    // parity: `swapSingleItem` ends in `setChanged(ITEM_INTERACT_FINISH)`. Its one exception, an item
    // whose `use_effects` asks for silence, is a component this server does not carry yet.
    if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
        world::vibrations::emit(world.state(), "item_interact_finish", x + 0.5, y + 0.5, z + 0.5, world.gameTime(),
                                world::triggerFacts::vibrationSourceOf(player, context.triggers));
    }
    if (context.blocks != nullptr) {
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, *stateId, result->effects);
    }
}

bool useJukeboxFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
                   const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                   const std::optional<storage::StoredItemStack>& held) {
    // parity: `JukeboxBlock.useWithoutItem` pops what is playing before it looks at the hand at all.
    if (const std::optional<std::pair<world::BlockChange, wire::common::Identifier>> ejected =
            world::jukeboxes::eject(world.state(), x, y, z, world.gameTime());
        ejected.has_value()) {
        const world::BlockChange& change = ejected->first;
        announceBlock(context, world.name(), change.x, change.y, change.z, change.stateId);
        if (context.blocks != nullptr) {
            context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, change.stateId, change.effects);
        }
        spawnDroppedItem(context, world, ejected->second, 1, x + 0.5, y + 1.0, z + 0.5, 0.0, 0.0, 0.0, 0,
                         wire::nbt::NbtCompound{});
        notifyAround(context, world, x, y, z, std::nullopt);
        return true;
    }
    const std::optional<wire::common::Identifier> item =
        held.has_value() ? parsedOrNothing(held->id) : std::optional<wire::common::Identifier>{};
    if (!item.has_value() || !world::jukeboxes::isDisc(*item)) {
        return false;
    }
    const std::optional<world::BlockChange> change = world::jukeboxes::insert(world.state(), x, y, z, *item);
    if (!change.has_value()) {
        return false;
    }
    announceBlock(context, world.name(), change->x, change->y, change->z, change->stateId);
    if (context.blocks != nullptr) {
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, change->stateId, change->effects);
    }
    spendOne(context, player, heldSlot, *held);
    notifyAround(context, world, x, y, z, std::nullopt);
    // parity: `JukeboxPlayable.tryInsertIntoJukebox`.
    statAwards::award(player, "play_record");
    return true;
}

bool placeBookOnLecternFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                           const std::int32_t x, const std::int32_t y, const std::int32_t z,
                           const std::int32_t heldSlot, const std::optional<storage::StoredItemStack>& held) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const std::optional<wire::common::Identifier> item =
        held.has_value() ? parsedOrNothing(held->id) : std::optional<wire::common::Identifier>{};
    if (!stateId.has_value() || !item.has_value() || !world::lecterns::isBook(*item)) {
        return false;
    }
    if (server::registry::blockStateDefinitions::propertiesOf(*stateId)["has_book"] == "true") {
        return false;
    }
    // parity: `tryPlaceBook` takes one off the stack, and the pages a comparator counts are one of
    // its components.
    const std::optional<world::BlockChange> change = world::lecterns::placeBook(
        world.state(), x, y, z,
        server::world::loot::ItemStack{.item = *item, .count = 1, .components = held->components});
    if (!change.has_value()) {
        return false;
    }
    announceBlock(context, world.name(), change->x, change->y, change->z, change->stateId);
    // parity: the book goes on the lectern whatever the gamemode, unlike most of what a hand spends.
    storage::StoredItemStack left = *held;
    left.count = held->count - 1;
    player.inventory.set(heldSlot, held->count <= 1 ? std::nullopt : std::optional{left});
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
    notifyAround(context, world, x, y, z, std::nullopt);
    return true;
}

} // namespace basalt::server::runtime::player
