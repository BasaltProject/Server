#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerBlockItemUse.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerBlockToggle.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/EndPortals.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/server/runtime/world/RespawnAnchors.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/Waxing.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using play::BlockPos;
using play::Gamemode;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;

std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

storage::StoredItemStack withCount(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

} // namespace

void BlockInteractionHandler::primeTntAt(PlayerSession& session, const BlockPos& pos, std::int32_t heldSlot,
                                         const storage::StoredItemStack& held, const bool wears) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::primeTntFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held, wears);
    }
}

void BlockInteractionHandler::igniteAt(PlayerSession& session, const BlockPos& pos, std::int32_t heldSlot,
                                       const storage::StoredItemStack& held) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::igniteFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
    }
}

bool BlockInteractionHandler::useBoneMeal(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                          const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr &&
           player::useBoneMealFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
}

bool BlockInteractionHandler::useHoneycomb(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                           const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr &&
           player::useHoneycombFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
}

bool BlockInteractionHandler::useAxe(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                     const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr && player::useAxeFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
}

bool BlockInteractionHandler::chargeAnchor(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                           const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<world::BlockChange> charged =
        world == nullptr ? std::nullopt : world::respawnAnchors::charge(world->state(), pos.x, pos.y, pos.z);
    if (!charged.has_value()) {
        return false;
    }
    broadcastBlockUpdate(pos, charged->stateId);
    // parity: `setBlock(pos, state, 3)` plus `GameEvent.BLOCK_CHANGE`, and the comparator beside it
    // reads the charge, so the neighbours have to hear about it.
    notifyNeighbours(pos);
    heardBySculk("block_change", pos);
    if (session.gamemode == Gamemode::Creative) {
        return true;
    }
    session.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
    owner_->sendInventorySlot(heldSlot);
    return true;
}

void BlockInteractionHandler::placeEnderEye(PlayerSession& session, const BlockPos& pos, std::int32_t heldSlot,
                                            const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::vector<world::BlockChange> changed = world::endPortals::placeEye(world->state(), pos.x, pos.y, pos.z);
    if (changed.empty()) {
        return;
    }
    for (const world::BlockChange& change : changed) {
        broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
    }
    if (session.gamemode == Gamemode::Creative) {
        return;
    }
    session.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
    owner_->sendInventorySlot(heldSlot);
}

void BlockInteractionHandler::handleUseItemOn(const play::ServerboundUseItemOnPacket& useItemOn) {
    const auto ack = [this, &useItemOn] {
        owner_->sendCompressed(play::BlockChangedAckPacket::kId, [&useItemOn](wire::io::ByteBuffer& buffer) {
            play::BlockChangedAckPacket{.sequence = useItemOn.sequence}.write(buffer);
        });
    };

    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || useItemOn.hand != play::Hand::MainHand || session->gamemode == Gamemode::Spectator ||
        session->gamemode == Gamemode::Adventure) {
        ack();
        return;
    }

    const play::BlockHitResult& hit = useItemOn.blockHit;
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    const std::optional<std::int32_t> clickedState = owner_->world().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    // Asked before the click does anything at all: opening a chest, using a door and placing a block
    // are all this one question.
    if (hooks_.interactAllowed &&
        !hooks_.interactAllowed(*session, hit.pos.x, hit.pos.y, hit.pos.z, held.has_value() ? held->id : "")) {
        // The client predicted whatever it was about to do: put the block and the hand back.
        owner_->sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ClientboundBlockUpdatePacket{.pos = hit.pos, .blockStateId = clickedState.value_or(0)}.write(buffer);
        });
        owner_->sendInventory();
        ack();
        return;
    }

    // parity: `ServerPlayerGameMode.useItemOn` runs the block's own use before it considers the item
    // as something to place, and sneaking overrides that and places instead.
    if (!session->sneaking) {
        const Identifier* clicked =
            clickedState.has_value() ? reg::blockStateDefinitions::blockOf(*clickedState) : nullptr;
        const std::string path = clicked == nullptr ? std::string{} : std::string{clicked->path()};
        // parity: `RedStoneOreBlock.useItemOn` lights the ore and then answers `PASS` whenever the
        // held item is a block that could go down, so the ore lights **and** the block is placed.
        // This is why it falls through rather than acking here.
        if (path == "redstone_ore" || path == "deepslate_redstone_ore") {
            if (world::LoadedWorld* lit = owner_->currentWorld(); lit != nullptr) {
                if (const std::optional<world::BlockChange> glow =
                        world::miscGrowth::lightRedstoneOre(lit->state(), hit.pos.x, hit.pos.y, hit.pos.z);
                    glow.has_value()) {
                    broadcastBlockUpdate(hit.pos, glow->stateId);
                    notifyNeighbours(hit.pos);
                }
            }
        }
        if (path == "crafting_table") {
            owner_->menus().openCraftingTable(*session);
            ack();
            return;
        }
        if (!path.empty() && world::furnaces::cookerFor(path).has_value()) {
            owner_->menus().openFurnace(*session, hit.pos, path);
            ack();
            return;
        }
        if (!path.empty() && world::containers::isPlain(path)) {
            owner_->menus().openChest(*session, hit.pos, path);
            ack();
            return;
        }
        if (path == "crafter") {
            owner_->menus().openCrafter(*session, hit.pos);
            ack();
            return;
        }
        // parity: `CommandBlock.useWithoutItem`, which is `player.openCommandBlock(entity)` for
        // anyone allowed to and nothing at all for anyone else. The client opens the screen itself,
        // so what the server owes it is the block entity's own data.
        if (world::commandBlocks::modeOf(path).has_value()) {
            if (mayUseGameMasterBlocks(*session)) {
                sendBlockEntity(hit.pos);
                ack();
                return;
            }
        }
        // parity: `RespawnAnchorBlock.useItemOn`, which takes a piece of glowstone until the fourth
        // charge. Clicking it with anything else is the spawn point, which is not this.
        if (path == "respawn_anchor" && held.has_value() &&
            parsedOrNothing(held->id) == Identifier::vanilla("glowstone") &&
            chargeAnchor(*session, hit.pos, heldSlot, *held)) {
            ack();
            return;
        }
        // parity: `JukeboxBlock.useItemOn`/`useWithoutItem`: a disc goes in, and clicking a playing
        // one takes it back out.
        if (path == "jukebox" && useJukebox(*session, hit.pos)) {
            ack();
            return;
        }
        // parity: `LecternBlock.useItemOn`, which takes a book, and `useWithoutItem`, which opens
        // the one already there.
        if (path == "lectern" && useLectern(*session, hit.pos)) {
            ack();
            return;
        }
        // parity: `ComposterBlock.useItemOn` and `useWithoutItem`: one compostable item in, or the
        // bone meal out once it is ready.
        if (path == "composter" && useComposter(*session, hit.pos)) {
            ack();
            return;
        }
        // parity: `CampfireBlock.useItemOn`, one piece of food and nothing a campfire cannot cook.
        if (world::campfires::isCampfire(path) && placeOnCampfire(*session, hit.pos)) {
            ack();
            return;
        }
        // parity: `SmithingTableBlock.useWithoutItem`, whose four slots belong to the screen.
        if (path == "smithing_table") {
            owner_->menus().openSmithingMenu(*session);
            ack();
            return;
        }
        if (path == "stonecutter") {
            owner_->menus().openStonecutterMenu(*session);
            ack();
            return;
        }
        // parity: `AnvilBlock.useWithoutItem`, whose three slots belong to the screen and whose
        // block wears out under it.
        if (path == "anvil" || path == "chipped_anvil" || path == "damaged_anvil") {
            owner_->menus().openAnvilMenu(*session, hit.pos);
            ack();
            return;
        }
        // parity: `EnderChestBlock.useWithoutItem`, the player's own slots and not the block's.
        if (path == "ender_chest") {
            owner_->menus().openEnderChest(*session, hit.pos);
            ack();
            return;
        }
        if (world::chiseledBookshelves::isBookshelf(path)) {
            useBookshelf(*session, hit);
            ack();
            return;
        }
        // parity: `ShelfBlock.useItemOn`, one slot by hand and the whole hotbar when powered.
        if (world::shelves::isShelf(path)) {
            useShelf(*session, hit);
            ack();
            return;
        }
        if (world::decoratedPots::isPot(path)) {
            usePot(*session, hit.pos);
            ack();
            return;
        }
        if (path == "note_block") {
            if (world::LoadedWorld* loaded = owner_->currentWorld(); loaded != nullptr) {
                player::tuneNoteBlockFor(breakContext(), *loaded, hit.pos.x, hit.pos.y, hit.pos.z);
            }
            ack();
            return;
        }
        // parity: `BellBlock.useWithoutItem`, which only rings from a side that can swing it.
        if (path == "bell" && ringBell(hit)) {
            ack();
            return;
        }
        if (clickedState.has_value() && world::beds::isBed(*clickedState)) {
            useBed(*session, hit.pos, *clickedState);
            ack();
            return;
        }
        if (clickedState.has_value() && world::blockInteractions::isToggleable(*clickedState)) {
            toggleBlock(hit.pos, session->yaw);
            ack();
            return;
        }
        // parity: `RepeaterBlock.useWithoutItem` and `ComparatorBlock.useWithoutItem`.
        if (clickedState.has_value() && world::blockInteractions::isCycleable(*clickedState)) {
            cycleBlock(hit.pos);
            ack();
            return;
        }
        // Clicking a sign that is not waxed opens it for editing again.
        if (clickedState.has_value() && world::signs::isSign(*clickedState) && editSign(hit.pos)) {
            ack();
            return;
        }
    }

    const std::optional<Identifier> heldBlock = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    // Flint and steel is not a block, so it has to be answered before the "is this placeable" check
    // below throws the click away. parity: `FlintAndSteelItem.useOn`.
    // parity: `TntBlock.useItemOn` runs before anything a flint and steel would do to the square
    // beside it: the block is primed on the spot and taken away, and no fire is ever placed. A fire
    // charge does the same and is spent instead of worn.
    const bool lighter = held.has_value() && heldBlock == Identifier::vanilla("flint_and_steel");
    const bool charge = held.has_value() && heldBlock == Identifier::vanilla("fire_charge");
    if ((lighter || charge) && clickedState.has_value() &&
        reg::blockStateDefinitions::blockOf(*clickedState) != nullptr &&
        reg::blockStateDefinitions::blockOf(*clickedState)->path() == "tnt") {
        primeTntAt(*session, hit.pos, heldSlot, *held, lighter);
        ack();
        return;
    }
    if (lighter) {
        igniteAt(*session, hit.pos.offset(hit.direction), heldSlot, *held);
        ack();
        return;
    }
    // parity: `BoneMealItem.useOn`, which acts on the block clicked rather than the square beside it.
    if (held.has_value() && heldBlock == Identifier::vanilla("bone_meal") &&
        useBoneMeal(*session, hit.pos, heldSlot, *held)) {
        ack();
        return;
    }
    // parity: `ItemFrameItem.useOn`, which hangs it on the face clicked rather than putting a block.
    if (held.has_value() && heldBlock.has_value() && heldBlock->path().ends_with("item_frame") &&
        hangFrame(*session, hit, heldSlot, *held, *heldBlock)) {
        ack();
        return;
    }
    // parity: `HoneycombItem.useOn` and `AxeItem.useOn`, both of which change the block clicked.
    if (held.has_value() && heldBlock == Identifier::vanilla("honeycomb") &&
        useHoneycomb(*session, hit.pos, heldSlot, *held)) {
        ack();
        return;
    }
    if (held.has_value() && heldBlock->path().ends_with("_axe") && useAxe(*session, hit.pos, heldSlot, *held)) {
        ack();
        return;
    }
    // parity: `EnderEyeItem.useOn`, which acts on the frame itself rather than next to it.
    if (held.has_value() && heldBlock == Identifier::vanilla("ender_eye")) {
        placeEnderEye(*session, hit.pos, heldSlot, *held);
        ack();
        return;
    }
    if (!held.has_value() || !heldBlock.has_value()) {
        ack();
        return;
    }
    // parity: `ServerPlayerGameMode.useItemOn` falls through to using the item itself, which is what
    // lets anyone eat while looking at the ground rather than only at the sky.
    if (owner_->itemUseTimers().startEating(*session, *heldBlock, heldSlot)) {
        ack();
        return;
    }
    // parity: `MinecartItem.useOn`, which only works on a rail and sits the cart on it.
    if (heldBlock->path().ends_with("minecart") && placeMinecart(*session, *heldBlock, heldSlot, *held, hit)) {
        ack();
        return;
    }
    // parity: `BoatItem.use`, which puts the boat where the player is looking.
    const bool isBoat = heldBlock->path().ends_with("_boat") || heldBlock->path() == "bamboo_raft" ||
                        heldBlock->path() == "bamboo_chest_raft";
    if (isBoat && placeBoat(*session, *heldBlock, heldSlot, *held, hit)) {
        ack();
        return;
    }
    // parity: `SpawnEggItem.useOn`, which drops the mob on top of what was clicked.
    if (heldBlock->path().ends_with("_spawn_egg") && useSpawnEgg(*session, *heldBlock, heldSlot, *held, hit)) {
        ack();
        return;
    }
    // A bucket aimed at a block: the client only sends `use_item` when nothing is in the way, so
    // without this, water poured anywhere near a wall went nowhere.
    if (world::buckets::isBucket(*heldBlock)) {
        useBucketOn(*session, *heldBlock, heldSlot, *held, hit);
        ack();
        return;
    }

    // From here the block simply goes down, which is the same everywhere and lives where a player
    // with no `ClientSession` can reach it.
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        ack();
        return;
    }
    player::PlaceContext placing;
    placing.blocks = breakContext();
    placing.placeAllowed = hooks_.placeAllowed;
    placing.nextBool = options_.nextBool;
    placing.sendInventory = [this](const PlayerSession&) { owner_->sendInventory(); };
    placing.openSign = [this](const PlayerSession&, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                              const std::int32_t stateId) { openNewSign(BlockPos{x, y, z}, stateId); };
    const player::BlockHit where{.x = hit.pos.x,
                                 .y = hit.pos.y,
                                 .z = hit.pos.z,
                                 .direction = hit.direction,
                                 .clickX = hit.clickX,
                                 .clickY = hit.clickY,
                                 .clickZ = hit.clickZ,
                                 .inside = hit.inside};
    static_cast<void>(player::placeBlockFor(*session, *world, where, *heldBlock, heldSlot, *held, placing));
    ack();
}

void BlockInteractionHandler::handleUseItem(const play::ServerboundUseItemPacket& useItem) {
    // The ack comes first and unconditionally: the client already changed its own copy, and the ack
    // is what undoes it when the server disagrees.
    owner_->sendCompressed(play::BlockChangedAckPacket::kId, [&useItem](wire::io::ByteBuffer& buffer) {
        play::BlockChangedAckPacket{.sequence = useItem.sequence}.write(buffer);
    });
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr || useItem.hand != play::Hand::MainHand ||
        session->gamemode == Gamemode::Spectator || session->gamemode == Gamemode::Adventure) {
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!item.has_value()) {
        return;
    }
    // parity: `ItemCooldowns.isOnCooldown`, which refuses the use outright rather than letting it
    // through and greying the slot afterwards.
    if (owner_->itemUseTimers().onCooldown(*item) || owner_->itemUseTimers().startEating(*session, *item, heldSlot) ||
        owner_->itemUseTimers().startDrawing(*session, *item, heldSlot) ||
        owner_->itemUseTimers().throwEnderPearl(*session, *item, heldSlot) || !world::buckets::isBucket(*item)) {
        return;
    }

    static_cast<void>(player::useBucketFor(breakContext(), *session, *world, *item, heldSlot, *held, useItem.yaw,
                                           useItem.pitch, hooks_.bucketAllowed));
}

} // namespace basalt::server::runtime
