#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/WitherSystem.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerBeehives.hpp>
#include <basalt/server/runtime/player/PlayerBlockItemUse.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerBlockToggle.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerCakesAndPots.hpp>
#include <basalt/server/runtime/player/PlayerCauldrons.hpp>
#include <basalt/server/runtime/player/PlayerFireworks.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSpawnItems.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/BlockTransformers.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/EndPortals.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
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
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/server/runtime/world/RespawnAnchors.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
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

void BlockInteractionHandler::awardItemUsedOn(const PlayerSession& session, const BlockPos& pos,
                                              const std::optional<std::int32_t>& clickedState,
                                              const std::optional<storage::StoredItemStack>& tool) const {
    awardBlockUse(session, pos, clickedState, tool, false);
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    world::LoadedWorld* world = owner_->currentWorld();
    world::RealLootTables* tables = owner_->options().lootTables;
    if (triggers == nullptr || world == nullptr || tables == nullptr || !clickedState.has_value() ||
        !triggers->listensTo("item_used_on_block")) {
        return;
    }
    triggers->fire("item_used_on_block", session.name(), session.uuid().toString(),
                   world::TriggerEvent{.location = world::triggerFacts::atBlock(*tables, *world, session, pos.x, pos.y,
                                                                                pos.z, tool, clickedState)});
}

void BlockInteractionHandler::awardBlockUse(const PlayerSession& session, const BlockPos& pos,
                                            const std::optional<std::int32_t>& clickedState,
                                            const std::optional<storage::StoredItemStack>& tool,
                                            bool withoutItem) const {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    world::LoadedWorld* world = owner_->currentWorld();
    world::RealLootTables* tables = owner_->options().lootTables;
    const std::string_view trigger = withoutItem ? "default_block_use" : "any_block_use";
    if (triggers == nullptr || world == nullptr || tables == nullptr || !clickedState.has_value() ||
        !triggers->listensTo(trigger)) {
        return;
    }
    triggers->fire(trigger, session.name(), session.uuid().toString(),
                   world::TriggerEvent{.location = world::triggerFacts::atBlock(*tables, *world, session, pos.x, pos.y,
                                                                                pos.z, tool, clickedState)});
}

void BlockInteractionHandler::handleUseItemOn(const play::ServerboundUseItemOnPacket& useItemOn) {
    const auto ack = [this, &useItemOn] {
        owner_->sendCompressed(play::BlockChangedAckPacket::kId, [&useItemOn](wire::io::ByteBuffer& buffer) {
            play::BlockChangedAckPacket{.sequence = useItemOn.sequence}.write(buffer);
        });
    };

    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || session->gamemode == Gamemode::Spectator || session->gamemode == Gamemode::Adventure) {
        ack();
        return;
    }

    const play::BlockHitResult& hit = useItemOn.blockHit;
    // parity: a client whose main hand did nothing asks again with the off hand, and then it is that
    // stack that goes down. The block has already been offered the main hand's click.
    const bool mainHand = useItemOn.hand == play::Hand::MainHand;
    const std::int32_t heldSlot =
        mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    const std::optional<std::int32_t> clickedState = owner_->world().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    // parity: `DEFAULT_BLOCK_USE` from `ServerPlayerGameMode.useItemOn` and `ANY_BLOCK_USE` from
    // `handleUseItemOn`, for a block's own use that took the click.
    const auto blockUsed = [&] {
        awardBlockUse(*session, hit.pos, clickedState, std::nullopt, true);
        awardBlockUse(*session, hit.pos, clickedState, held, false);
        ack();
    };
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
    if (!session->sneaking && mainHand) {
        const Identifier* clicked =
            clickedState.has_value() ? reg::blockStateDefinitions::blockOf(*clickedState) : nullptr;
        const std::string path = clicked == nullptr ? std::string{} : std::string{clicked->path()};
        // parity: each screen block's `useWithoutItem` awards its statistic once the screen opens.
        const std::int32_t containerIdBefore = owner_->menus().nextContainerId;
        const auto awardOpened = [this, session, &path, containerIdBefore] {
            const std::optional<std::string_view> stat = player::statAwards::openStatOf(path);
            if (stat.has_value() && owner_->menus().nextContainerId != containerIdBefore) {
                player::statAwards::award(*session, *stat);
            }
        };
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
        // parity: `FenceBlock.useWithoutItem`, the player's mobs tied to a knot on it.
        if (clicked != nullptr && owner_->options().mobs != nullptr && owner_->options().blockTags != nullptr &&
            owner_->currentWorld() != nullptr &&
            world::realBlockTags::contains(*owner_->options().blockTags, "minecraft:fences", *clicked) &&
            owner_->options().mobs->bindPlayerMobs(*owner_->currentWorld(), *session,
                                                   {hit.pos.x, hit.pos.y, hit.pos.z})) {
            blockUsed();
            return;
        }
        if (path == "crafting_table") {
            owner_->menus().openCraftingTable(*session);
            awardOpened();
            blockUsed();
            return;
        }
        if (!path.empty() && world::furnaces::cookerFor(path).has_value()) {
            owner_->menus().openFurnace(*session, hit.pos, path);
            awardOpened();
            blockUsed();
            return;
        }
        if (path == "brewing_stand") {
            owner_->menus().openBrewingStand(*session, hit.pos);
            awardOpened();
            blockUsed();
            return;
        }
        if (!path.empty() && world::containers::isPlain(path)) {
            owner_->menus().openChest(*session, hit.pos, path);
            awardOpened();
            // parity: `ChestBlock`, `BarrelBlock` and `ShulkerBoxBlock`, which anger the piglins that see it.
            if (path != "hopper" && path != "dispenser" && path != "dropper" && owner_->options().mobs != nullptr &&
                owner_->currentWorld() != nullptr) {
                owner_->options().mobs->angerNearbyPiglins(*owner_->currentWorld(), *session, true);
            }
            blockUsed();
            return;
        }
        if (path == "crafter") {
            owner_->menus().openCrafter(*session, hit.pos);
            blockUsed();
            return;
        }
        // parity: `CommandBlock.useWithoutItem`, which is `player.openCommandBlock(entity)` for
        // anyone allowed to and nothing at all for anyone else. The client opens the screen itself,
        // so what the server owes it is the block entity's own data.
        // parity: `StructureBlockEntity.usedBy`, the same: the client opens its own screen.
        if (world::commandBlocks::modeOf(path).has_value() || path == "structure_block") {
            if (mayUseGameMasterBlocks(*session)) {
                sendBlockEntity(hit.pos);
                blockUsed();
                return;
            }
        }
        // parity: `RespawnAnchorBlock.useItemOn`, which takes a piece of glowstone until the fourth
        // charge. Clicking it with anything else is the spawn point, which is not this.
        if (path == "respawn_anchor" && held.has_value() &&
            parsedOrNothing(held->id) == Identifier::vanilla("glowstone") &&
            chargeAnchor(*session, hit.pos, heldSlot, *held)) {
            blockUsed();
            return;
        }
        // parity: `JukeboxBlock.useItemOn`/`useWithoutItem`: a disc goes in, and clicking a playing
        // one takes it back out.
        if (path == "jukebox" && useJukebox(*session, hit.pos)) {
            blockUsed();
            return;
        }
        // parity: `LecternBlock.useItemOn`, which takes a book, and `useWithoutItem`, which opens
        // the one already there.
        if (path == "lectern" && useLectern(*session, hit.pos)) {
            blockUsed();
            return;
        }
        // parity: `ComposterBlock.useItemOn` and `useWithoutItem`: one compostable item in, or the
        // bone meal out once it is ready.
        if (path == "composter" && useComposter(*session, hit.pos)) {
            blockUsed();
            return;
        }
        // parity: `BeehiveBlock.useItemOn`, shears or a bottle on a full hive.
        if (world::beehives::isHive(path)) {
            if (world::LoadedWorld* world = owner_->currentWorld();
                world != nullptr && player::harvestHive(breakContext(), *session, *world, hit.pos.x, hit.pos.y,
                                                        hit.pos.z, heldSlot, held, owner_->options().mobs)) {
                blockUsed();
                return;
            }
        }
        // parity: `CampfireBlock.useItemOn`, one piece of food and nothing a campfire cannot cook.
        if (world::campfires::isCampfire(path) && placeOnCampfire(*session, hit.pos)) {
            blockUsed();
            return;
        }
        // parity: `SmithingTableBlock.useWithoutItem`, whose four slots belong to the screen.
        if (path == "smithing_table") {
            owner_->menus().openSmithingMenu(*session);
            awardOpened();
            blockUsed();
            return;
        }
        if (path == "stonecutter") {
            owner_->menus().openStonecutterMenu(*session);
            awardOpened();
            blockUsed();
            return;
        }
        // parity: `AnvilBlock.useWithoutItem`, whose three slots belong to the screen and whose
        // block wears out under it.
        // parity: `EnchantingTableBlock.useWithoutItem`, whose two slots belong to the screen and
        // whose offers come off the shelves around it.
        if (path == "enchanting_table") {
            owner_->menus().openEnchantmentMenu(*session, hit.pos);
            blockUsed();
            return;
        }
        // parity: `CartographyTableBlock.useWithoutItem`.
        if (path == "cartography_table") {
            owner_->menus().openCartographyTable(*session, hit.pos);
            awardOpened();
            blockUsed();
            return;
        }
        // parity: `LoomBlock.useWithoutItem`, whose four slots belong to the screen.
        if (path == "loom") {
            owner_->menus().openLoomMenu(*session, hit.pos);
            awardOpened();
            blockUsed();
            return;
        }
        // parity: `BeaconBlockEntity` is a `MenuProvider`, so the block just opens its screen.
        if (path == "beacon") {
            owner_->menus().openBeaconMenu(*session, hit.pos);
            awardOpened();
            blockUsed();
            return;
        }
        // parity: `GrindstoneBlock.useWithoutItem`, whose three slots belong to the screen.
        if (path == "grindstone") {
            owner_->menus().openGrindstoneMenu(*session, hit.pos);
            awardOpened();
            blockUsed();
            return;
        }
        if (path == "anvil" || path == "chipped_anvil" || path == "damaged_anvil") {
            owner_->menus().openAnvilMenu(*session, hit.pos);
            awardOpened();
            blockUsed();
            return;
        }
        // parity: `EnderChestBlock.useWithoutItem`, the player's own slots and not the block's.
        if (path == "ender_chest") {
            owner_->menus().openEnderChest(*session, hit.pos);
            awardOpened();
            if (owner_->options().mobs != nullptr && owner_->currentWorld() != nullptr) {
                owner_->options().mobs->angerNearbyPiglins(*owner_->currentWorld(), *session, true);
            }
            blockUsed();
            return;
        }
        if (world::chiseledBookshelves::isBookshelf(path)) {
            useBookshelf(*session, hit);
            blockUsed();
            return;
        }
        // parity: `ShelfBlock.useItemOn`, one slot by hand and the whole hotbar when powered.
        if (world::shelves::isShelf(path)) {
            useShelf(*session, hit);
            blockUsed();
            return;
        }
        if (world::decoratedPots::isPot(path)) {
            usePot(*session, hit.pos);
            blockUsed();
            return;
        }
        // parity: `AbstractCauldronBlock.useItemOn`, whatever the held item does to one.
        if (path.ends_with("cauldron") && player::useCauldronFor(breakContext(), *session, *owner_->currentWorld(),
                                                                 hit.pos.x, hit.pos.y, hit.pos.z, heldSlot, held)) {
            blockUsed();
            return;
        }
        // parity: `CakeBlock` and `CandleCakeBlock`, a slice or a candle.
        if ((path == "cake" || path.ends_with("candle_cake")) &&
            player::useCakeFor(breakContext(), *session, *owner_->currentWorld(), hit.pos.x, hit.pos.y, hit.pos.z,
                               heldSlot, held, hit.clickY)) {
            blockUsed();
            return;
        }
        // parity: `FlowerPotBlock`, a plant in or a plant out.
        if ((path == "flower_pot" || path.starts_with("potted_")) &&
            player::useFlowerPotFor(breakContext(), *session, *owner_->currentWorld(), hit.pos.x, hit.pos.y, hit.pos.z,
                                    heldSlot, held)) {
            blockUsed();
            return;
        }
        if (path == "note_block") {
            if (world::LoadedWorld* loaded = owner_->currentWorld(); loaded != nullptr) {
                player::tuneNoteBlockFor(breakContext(), *loaded, hit.pos.x, hit.pos.y, hit.pos.z, session);
            }
            blockUsed();
            return;
        }
        // parity: `BellBlock.useWithoutItem`, which only rings from a side that can swing it.
        if (path == "bell" && ringBell(hit)) {
            blockUsed();
            return;
        }
        if (clickedState.has_value() && world::beds::isBed(*clickedState)) {
            useBed(*session, hit.pos, *clickedState);
            blockUsed();
            return;
        }
        if (clickedState.has_value() && world::blockInteractions::isToggleable(*clickedState)) {
            toggleBlock(hit.pos, session->yaw);
            blockUsed();
            return;
        }
        // parity: `RepeaterBlock.useWithoutItem` and `ComparatorBlock.useWithoutItem`. The statue is
        // the one that answers PASS to an axe, and the axe branch is further down, so it has to let
        // that hand through rather than turn a pose.
        const std::optional<Identifier> inHand = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
        const bool axeInHand = inHand.has_value() && inHand->path().ends_with("_axe");
        const wire::common::Identifier* clickedBlock =
            clickedState.has_value() ? reg::blockStateDefinitions::blockOf(*clickedState) : nullptr;
        // parity: `WeatheringCopperGolemStatueBlock.useItemOn`: an axe on one that never weathered
        // wakes the golem, and a honeycomb is left to wax it.
        if (axeInHand && clickedBlock != nullptr && clickedBlock->path() == "copper_golem_statue") {
            MobActionSystem* mobs = owner_->options().mobs;
            world::LoadedWorld* here = owner_->currentWorld();
            if (mobs != nullptr && here != nullptr && mobs->awakenStatue(*here, hit.pos.x, hit.pos.y, hit.pos.z)) {
                wearHeldItem(1);
            }
            blockUsed();
            return;
        }
        const bool waxesStatue = inHand == Identifier::vanilla("honeycomb") && clickedBlock != nullptr &&
                                 clickedBlock->path().ends_with("copper_golem_statue") &&
                                 !clickedBlock->path().starts_with("waxed_");
        if (clickedState.has_value() && world::blockInteractions::isCycleable(*clickedState) && !waxesStatue &&
            !(axeInHand && world::blockInteractions::cycleYieldsToAxe(*clickedState))) {
            cycleBlock(hit.pos);
            blockUsed();
            return;
        }
        // Clicking a sign that is not waxed opens it for editing again.
        if (clickedState.has_value() && world::signs::isSign(*clickedState) && editSign(hit.pos)) {
            blockUsed();
            return;
        }
    }

    // parity: `MapItem.useOn`, which marks a banner and fails on one it cannot mark.
    if (MapSystem* maps = owner_->options().maps; maps != nullptr) {
        if (world::LoadedWorld* here = owner_->currentWorld(); here != nullptr) {
            if (maps->useOnBanner(*session, *here, heldSlot, hit.pos.x, hit.pos.y, hit.pos.z).has_value()) {
                ack();
                return;
            }
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
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `ItemFrameItem.useOn` and `PaintingItem.useOn`, which hang on the face clicked rather
    // than putting a block.
    if (held.has_value() && heldBlock.has_value() &&
        (heldBlock->path().ends_with("item_frame") || heldBlock->path() == "painting") &&
        hangFrame(*session, hit, heldSlot, *held, *heldBlock)) {
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `HoneycombItem.useOn` and `AxeItem.useOn`, both of which change the block clicked.
    // Both raise the criterion **before** writing the new state, so what it reads is the block the
    // player clicked: waxing is earned on the bare copper, not on the waxed one it becomes.
    if (held.has_value() && heldBlock == Identifier::vanilla("honeycomb") &&
        useHoneycomb(*session, hit.pos, heldSlot, *held)) {
        awardItemUsedOn(*session, hit.pos, clickedState, held);
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `Item.useOn` over the `block_transformer` a hoe or a shovel carries: tilling and paths.
    if (held.has_value() && heldBlock.has_value() && owner_->options().blockTransformers != nullptr &&
        (heldBlock->path().ends_with("_hoe") || heldBlock->path().ends_with("_shovel"))) {
        const std::optional<Identifier> transformer = world::RealBlockTransformers::defaultFor(*heldBlock);
        world::LoadedWorld* here = owner_->currentWorld();
        if (transformer.has_value() && here != nullptr &&
            player::transformBlockFor(breakContext(), *session, *here, *owner_->options().blockTransformers,
                                      *transformer, hit.pos.x, hit.pos.y, hit.pos.z, hit.direction, heldSlot, *held)) {
            awardItemUsedOn(*session, hit.pos, clickedState, held);
            player::statAwards::used(*session, held->id);
            ack();
            return;
        }
    }
    if (held.has_value() && heldBlock->path().ends_with("_axe") && useAxe(*session, hit.pos, heldSlot, *held)) {
        awardItemUsedOn(*session, hit.pos, clickedState, held);
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `FireworkRocketItem.useOn`, which launches from the face clicked.
    if (held.has_value() && heldBlock == Identifier::vanilla("firework_rocket") &&
        owner_->options().fireworks != nullptr && owner_->currentWorld() != nullptr &&
        player::useRocketOnFor(breakContext(), *owner_->options().fireworks, *session, *owner_->currentWorld(),
                               heldSlot,
                               player::BlockHit{.x = hit.pos.x,
                                                .y = hit.pos.y,
                                                .z = hit.pos.z,
                                                .direction = hit.direction,
                                                .clickX = hit.clickX,
                                                .clickY = hit.clickY,
                                                .clickZ = hit.clickZ})) {
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
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `BoatItem.use`, which puts the boat where the player is looking.
    const bool isBoat = heldBlock->path().ends_with("_boat") || heldBlock->path() == "bamboo_raft" ||
                        heldBlock->path() == "bamboo_chest_raft";
    if (isBoat && placeBoat(*session, *heldBlock, heldSlot, *held, hit)) {
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `CushionItem.useOn`, which answers `FAIL` rather than placing anything on a miss.
    if (const std::optional<std::string_view> colour = world::cushions::colourOfItem(*heldBlock); colour.has_value()) {
        if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
            static_cast<void>(player::placeCushionFor(breakContext(), *session, *world, *colour, heldSlot, *held,
                                                      player::BlockHit{.x = hit.pos.x,
                                                                       .y = hit.pos.y,
                                                                       .z = hit.pos.z,
                                                                       .direction = hit.direction,
                                                                       .clickX = hit.clickX,
                                                                       .clickY = hit.clickY,
                                                                       .clickZ = hit.clickZ}));
        }
        ack();
        return;
    }
    // parity: `EndCrystalItem.useOn`, and in a world with a dragon fight, `tryRespawn` after it.
    if (heldBlock == Identifier::vanilla("end_crystal")) {
        if (world::LoadedWorld* world = owner_->currentWorld();
            world != nullptr &&
            player::placeEndCrystalFor(breakContext(), *session, *world, heldSlot, *held,
                                       player::BlockHit{.x = hit.pos.x,
                                                        .y = hit.pos.y,
                                                        .z = hit.pos.z,
                                                        .direction = hit.direction,
                                                        .clickX = hit.clickX,
                                                        .clickY = hit.clickY,
                                                        .clickZ = hit.clickZ}) &&
            world->dragonFight() != nullptr && owner_->options().endFight != nullptr) {
            owner_->options().endFight->tryRespawn(*world);
        }
        ack();
        return;
    }
    // parity: `SpawnEggItem.useOn`, which drops the mob on top of what was clicked.
    if (heldBlock->path().ends_with("_spawn_egg") && useSpawnEgg(*session, *heldBlock, heldSlot, *held, hit)) {
        player::statAwards::used(*session, held->id);
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
    if (WitherSystem* withers = owner_->options().withers; withers != nullptr) {
        placing.placed = [withers](world::LoadedWorld& at, std::int32_t x, std::int32_t y, std::int32_t z) {
            withers->placed(at, x, y, z);
        };
    }
    const player::BlockHit where{.x = hit.pos.x,
                                 .y = hit.pos.y,
                                 .z = hit.pos.z,
                                 .direction = hit.direction,
                                 .clickX = hit.clickX,
                                 .clickY = hit.clickY,
                                 .clickZ = hit.clickZ,
                                 .inside = hit.inside};
    if (player::placeBlockFor(*session, *world, where, *heldBlock, heldSlot, *held, placing)) {
        awardBlockUse(*session, hit.pos, clickedState, held, false);
    }
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
    if (session == nullptr || world == nullptr || session->gamemode == Gamemode::Spectator) {
        return;
    }
    // parity: a client that got nothing from the main hand asks again with the off hand, and the
    // rod or the shield is what answers there.
    if (useItem.hand != play::Hand::MainHand) {
        const std::optional<storage::StoredItemStack> offhand = session->inventory.at(PlayerInventory::kOffhandSlot);
        const std::optional<Identifier> other = offhand.has_value() ? parsedOrNothing(offhand->id) : std::nullopt;
        if (!other.has_value() || owner_->itemUseTimers().onCooldown(*other)) {
            return;
        }
        // parity: `CrossbowItem.use` and `BowItem.use` answer either hand.
        if (owner_->itemUseTimers().startDrawing(*session, *other, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `FishingRodItem.use` answers either hand.
        if (owner_->options().fishing != nullptr &&
            owner_->options().fishing->useRod(breakContext(), *session, *world, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `EmptyMapItem.use` answers either hand.
        if (owner_->options().maps != nullptr &&
            owner_->options().maps->useEmptyMap(breakContext(), *session, *world, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `EnderEyeItem.use` answers either hand.
        if (world::enderEyes::throwFor(breakContext(), *session, *world, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `Item.use` charges a `kinetic_weapon` from either hand.
        if (owner_->combat().startCharge(*session, PlayerInventory::kOffhandSlot)) {
            return;
        }
        if (!owner_->itemUseTimers().startBlocking(*session, *other, PlayerInventory::kOffhandSlot)) {
            static_cast<void>(owner_->itemUseTimers().startSpyglass(*session, *other, PlayerInventory::kOffhandSlot));
        }
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!item.has_value()) {
        return;
    }
    // parity: `FireworkRocketItem.use` on a glider lets go of every lead first, snapping.
    if (session->fallFlying && item->path() == "firework_rocket" && !owner_->itemUseTimers().onCooldown(*item) &&
        owner_->options().mobs != nullptr && owner_->options().mobs->dropAllLeashes(*world, *session)) {
        owner_->broadcastSound("minecraft:entity.lead.break", session->x, session->y, session->z,
                               play::SoundSource::Neutral);
    }
    // parity: `ItemCooldowns.isOnCooldown`, which refuses the use outright rather than letting it
    // through and greying the slot afterwards.
    if (owner_->itemUseTimers().onCooldown(*item) || owner_->itemUseTimers().startEating(*session, *item, heldSlot) ||
        owner_->itemUseTimers().startDrawing(*session, *item, heldSlot) ||
        owner_->itemUseTimers().throwEnderPearl(*session, *item, heldSlot) ||
        world::enderEyes::throwFor(breakContext(), *session, *world, heldSlot) ||
        (owner_->options().fireworks != nullptr &&
         player::useRocketFor(breakContext(), *owner_->options().fireworks, *session, *world, heldSlot)) ||
        owner_->itemUseTimers().startBlocking(*session, *item, heldSlot) ||
        owner_->combat().startCharge(*session, heldSlot) ||
        owner_->itemUseTimers().startSpyglass(*session, *item, heldSlot) ||
        (owner_->options().fishing != nullptr &&
         owner_->options().fishing->useRod(breakContext(), *session, *world, heldSlot)) ||
        (owner_->options().maps != nullptr &&
         owner_->options().maps->useEmptyMap(breakContext(), *session, *world, heldSlot)) ||
        !world::buckets::isBucket(*item)) {
        return;
    }
    // parity: `BucketItem.use` asks `mayUseItemAt`, which an adventure player never may.
    if (session->gamemode == Gamemode::Adventure) {
        return;
    }

    static_cast<void>(player::useBucketFor(breakContext(), *session, *world, *item, heldSlot, *held, useItem.yaw,
                                           useItem.pitch, hooks_.bucketAllowed));
}

} // namespace basalt::server::runtime
