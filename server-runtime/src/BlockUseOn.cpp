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
#include <basalt/server/runtime/world/LoadedWorld.hpp>
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

/// parity: the `15` vanilla passes with `LevelEvent.PARTICLES_AND_SOUND_PLANT_GROWTH`.
constexpr std::int32_t kGrowthSparkles = 15;

storage::StoredItemStack withCount(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

/// The neighbour-update context this world runs its cascades with.
world::neighbourUpdates::Context contextOf(world::LoadedWorld& world, random::RandomSource& random) {
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    return world::neighbourUpdates::Context{.world = world.state(),
                                            .worldName = world.name(),
                                            .minY = facts.chunkHeight.minY(),
                                            .maxY = facts.chunkHeight.maxY(),
                                            .random = random,
                                            .hasSkylight = facts.hasSkylight,
                                            .gameTime = world.gameTime(),
                                            .tntExplodes = world.gameRules().get(world::gameRules::kTntExplodes)};
}

} // namespace

void BlockInteractionHandler::igniteAt(PlayerSession& session, const BlockPos& pos, std::int32_t heldSlot,
                                       const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> existing =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    if (!existing.has_value() || !isAirLike(*existing)) {
        return;
    }
    // parity: `BaseFireBlock.onPlace` checks the frame, not the item, which is why any fire in the
    // right place opens a portal and why the portal replaces the fire rather than burning next to it.
    const std::optional<server::world::portal::PortalShape> shape =
        world::netherPortals::light(*world, pos.x, pos.y, pos.z);
    if (shape.has_value()) {
        const std::int32_t portal = server::world::portal::PortalShape::portalState(shape->axis());
        for (const server::world::portal::PortalBlock& block : shape->interior()) {
            broadcastBlockUpdate(BlockPos{block.x, block.y, block.z}, portal);
        }
    } else {
        if (!world::netherPortals::placeFire(*world, pos.x, pos.y, pos.z)) {
            return;
        }
        const std::optional<std::int32_t> fire = world->state().blockState(pos.x, pos.y, pos.z);
        broadcastBlockUpdate(pos, fire.value_or(reg::blocks::air().stateId));
        // parity: `BaseFireBlock.onPlace`, which is what starts fire acting at all.
        random::RandomSource random = random::RandomSource::legacy(nextSeed());
        world::fires::schedule(world->state(), pos.x, pos.y, pos.z, random);
    }

    if (session.gamemode == Gamemode::Creative) {
        return;
    }
    random::RandomSource wear = random::RandomSource::legacy(nextSeed());
    const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
        held, 1, wear,
        [this](const storage::StoredItemStack& stack, std::int32_t amount, random::RandomSource& source) {
            return damageEffects_.process(stack, amount, source);
        });
    if (worn == held) {
        return;
    }
    session.inventory.set(heldSlot, worn);
    owner_->sendInventorySlot(heldSlot);
}

bool BlockInteractionHandler::useBoneMeal(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                          const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> existing =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    // parity: `BoneMealItem.growCrop` spends the item on a valid target whether or not the growth
    // roll did anything, and answers no on anything else so the click falls through.
    if (!existing.has_value() || !world::boneMeal::grows(*existing)) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    std::optional<world::BlockChange> grown = world::boneMeal::apply(world->state(), pos.x, pos.y, pos.z, random);
    if (grown.has_value()) {
        grown->effects.emplace_back(
            world::blockEffect::Effect{.type = world::blockEffect::Effect::kPlantGrowth, .data = kGrowthSparkles});
        BlockChangeBroadcaster* blocks = owner_->options().blocks;
        if (blocks != nullptr) {
            blocks->announceBlock(owner_->playersInSameWorld(), *grown);
        }
    }
    if (session.gamemode == Gamemode::Creative) {
        return true;
    }
    session.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
    owner_->sendInventorySlot(heldSlot);
    return true;
}

void BlockInteractionHandler::reshapeBlock(const BlockPos& pos, const std::int32_t stateId, const std::int32_t effect) {
    owner_->world().setBlockState(pos.x, pos.y, pos.z, stateId);
    world::BlockChange change{.x = pos.x, .y = pos.y, .z = pos.z, .stateId = stateId};
    change.effects.emplace_back(world::blockEffect::Effect{.type = effect, .data = 0});
    BlockChangeBroadcaster* blocks = owner_->options().blocks;
    if (blocks != nullptr) {
        blocks->announceBlock(owner_->playersInSameWorld(), change);
    }
    // parity: `setBlock(pos, state, 11)`, which does tell the neighbours, and `GameEvent.BLOCK_CHANGE`.
    notifyNeighbours(pos);
    heardBySculk("block_change", pos);
}

bool BlockInteractionHandler::useHoneycomb(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                           const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> existing =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    const std::optional<std::int32_t> waxed =
        existing.has_value() ? world::waxing::waxed(*existing) : std::optional<std::int32_t>{};
    if (!waxed.has_value()) {
        return false;
    }
    reshapeBlock(pos, *waxed, world::blockEffect::Effect::kWaxOn);
    if (session.gamemode == Gamemode::Creative) {
        return true;
    }
    session.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
    owner_->sendInventorySlot(heldSlot);
    return true;
}

bool BlockInteractionHandler::useAxe(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                     const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> existing =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    const std::optional<world::waxing::Scrape> cut =
        existing.has_value() ? world::waxing::scraped(*existing) : std::optional<world::waxing::Scrape>{};
    if (!cut.has_value()) {
        return false;
    }
    // parity: stripping a log is only a sound, and the other two draw their own particles.
    const std::int32_t effect = cut->cut == world::waxing::Cut::Scraped   ? world::blockEffect::Effect::kCopperScrape
                                : cut->cut == world::waxing::Cut::Unwaxed ? world::blockEffect::Effect::kWaxOff
                                                                          : 0;
    if (effect == 0) {
        owner_->world().setBlockState(pos.x, pos.y, pos.z, cut->stateId);
        broadcastBlockUpdate(pos, cut->stateId);
        notifyNeighbours(pos);
        heardBySculk("block_change", pos);
    } else {
        reshapeBlock(pos, cut->stateId, effect);
    }
    if (session.gamemode == Gamemode::Creative) {
        return true;
    }
    // parity: `hurtAndBreak(1, ...)`, one point off the axe whichever of the three it did.
    random::RandomSource wear = random::RandomSource::legacy(nextSeed());
    const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
        held, 1, wear,
        [this](const storage::StoredItemStack& stack, std::int32_t amount, random::RandomSource& source) {
            return damageEffects_.process(stack, amount, source);
        });
    if (worn == held) {
        return true;
    }
    session.inventory.set(heldSlot, worn);
    owner_->sendInventorySlot(heldSlot);
    return true;
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
        // parity: `NoteBlock.useWithoutItem`, which moves the note on one and plays it.
        if (path == "note_block") {
            world::LoadedWorld* loaded = owner_->currentWorld();
            if (loaded != nullptr) {
                const std::optional<world::BlockChange> change =
                    world::noteBlocks::tune(loaded->state(), hit.pos.x, hit.pos.y, hit.pos.z);
                if (change.has_value()) {
                    broadcastBlockUpdate(hit.pos, change->stateId);
                    broadcastEffects(hit.pos, change->stateId, change->effects);
                }
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
            toggleBlock(hit.pos);
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
    if (held.has_value() && heldBlock == Identifier::vanilla("flint_and_steel")) {
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

    // parity: `StandingAndWallBlockItem` and friends: a torch, a sign, a banner or a head put
    // against a side goes down as its wall form, not as the standing one refusing to stand.
    const bool againstAWall = hit.direction != play::Direction::Up && hit.direction != play::Direction::Down;
    // parity: `BlockItem.getBlock`, which is not the item's own name for redstone dust, string or
    // any seed.
    Identifier placedBlock = reg::itemBlocks::of(*heldBlock);
    if (againstAWall) {
        placedBlock = world::wallVariants::of(placedBlock).value_or(placedBlock);
    }
    const std::optional<std::int32_t> defaultStateId = reg::blocks::blockIdOf(placedBlock);
    if (!defaultStateId.has_value()) {
        ack();
        return;
    }

    world::LoadedWorld* world = owner_->currentWorld();
    // parity: `BlockPlaceContext`: the clicked block is replaced when it is one of the things that
    // give way (grass, a fern, a snow layer, fire, a fluid), and the block goes next to it otherwise.
    const bool replaceClicked = clickedState.has_value() && reg::blockShapes::isReplaceable(*clickedState) &&
                                reg::blockStateDefinitions::blockOf(*clickedState) != nullptr &&
                                *reg::blockStateDefinitions::blockOf(*clickedState) != *heldBlock;
    std::optional<std::array<std::int32_t, 3>> scaffoldTarget;
    if (heldBlock->path() == "scaffolding" && world != nullptr) {
        scaffoldTarget = world::scaffoldingPlacement::target(
            world->state(), world::netherPortals::minY(*world), world::netherPortals::maxY(*world), hit.pos.x,
            hit.pos.y, hit.pos.z, hit.direction, session->yaw, session->sneaking, hit.inside);
    }
    BlockPos placeAt = hit.pos.offset(hit.direction);
    if (scaffoldTarget.has_value()) {
        placeAt = BlockPos{(*scaffoldTarget)[0], (*scaffoldTarget)[1], (*scaffoldTarget)[2]};
    } else if (replaceClicked) {
        placeAt = hit.pos;
    }
    const std::optional<std::int32_t> existing = owner_->world().blockState(placeAt.x, placeAt.y, placeAt.z);
    if (!existing.has_value() || !reg::blockShapes::isReplaceable(*existing)) {
        ack();
        return;
    }

    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const std::int32_t placedStateId = world::blockPlacement::stateFor(
        placedBlock, *defaultStateId,
        world::blockPlacement::Placement{.face = hit.direction,
                                         .clickY = hit.clickY,
                                         .clickX = hit.clickX,
                                         .clickZ = hit.clickZ,
                                         .yaw = session->yaw,
                                         .pitch = session->pitch,
                                         .replacing = *existing,
                                         .world = world == nullptr ? nullptr : &world->state(),
                                         .x = placeAt.x,
                                         .y = placeAt.y,
                                         .z = placeAt.z},
        random);
    // The half a two-block one starts as, whatever the state table's first entry happens to be.
    const std::int32_t blockStateId = world::doubleBlocks::firstHalf(placedStateId);
    // parity: `BlockItem.canPlace`: a block never goes down inside somebody.
    const std::vector<std::array<double, 6>> builders = buildersInTheWay();
    if (world::blockPlacement::isObstructed(blockStateId, placeAt.x, placeAt.y, placeAt.z, builders)) {
        ack();
        return;
    }
    // parity: `canSurvive`, which refuses a torch hung on nothing rather than letting it be placed
    // and drop on the next update.
    if (world != nullptr &&
        !world::blockSurvival::isSupported(world->state(), placeAt.x, placeAt.y, placeAt.z, blockStateId)) {
        ack();
        return;
    }
    // parity: `setPlacedBy` on a door, a bed or a tall flower: the client asks for one block and the
    // game puts down two.
    const std::optional<world::SecondHalf> second =
        world::doubleBlocks::secondHalf(blockStateId, placeAt.x, placeAt.y, placeAt.z);
    if (second.has_value()) {
        const std::optional<std::int32_t> room = owner_->world().blockState(second->x, second->y, second->z);
        if (!room.has_value() || !reg::blockShapes::isReplaceable(*room)) {
            ack();
            return;
        }
    }
    // Asked once the server has decided the block would go down, and the client is told to put its
    // own prediction back when a plugin says no.
    const Identifier* placedName = reg::blockStateDefinitions::blockOf(blockStateId);
    if (hooks_.placeAllowed && !hooks_.placeAllowed(*session, placeAt.x, placeAt.y, placeAt.z,
                                                    placedName == nullptr ? "" : placedName->toString())) {
        ack();
        owner_->sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ClientboundBlockUpdatePacket{
                .pos = placeAt, .blockStateId = owner_->world().blockState(placeAt.x, placeAt.y, placeAt.z).value_or(0)}
                .write(buffer);
        });
        // The client took the item out of its own hand when it predicted the placement.
        owner_->sendInventory();
        return;
    }
    const bool changed = owner_->world().setBlockState(placeAt.x, placeAt.y, placeAt.z, blockStateId);
    ack();
    if (!changed) {
        return;
    }
    // parity: `getStateForPlacement` joins a fence, a wall or a staircase to what is already there,
    // and the announcement below has to carry the state it settled on.
    std::int32_t settledStateId = blockStateId;
    if (world != nullptr) {
        random::RandomSource joining = random::RandomSource::legacy(nextSeed());
        const world::neighbourUpdates::Context context = contextOf(*world, joining);
        settledStateId = world::neighbourUpdates::connect(context, placeAt.x, placeAt.y, placeAt.z, blockStateId)
                             .value_or(settledStateId);
        // parity: `BaseRailBlock.onPlace` into `RailState.place`, which bends the new rail and every
        // rail it decided to meet.
        const std::optional<std::int32_t> rail = world::rails::place(
            world->state(), world->name(), placeAt.x, placeAt.y, placeAt.z,
            [this, &placeAt, &settledStateId](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                if (x == placeAt.x && y == placeAt.y && z == placeAt.z) {
                    settledStateId = stateId;
                } else {
                    broadcastBlockUpdate(BlockPos{x, y, z}, stateId);
                }
            });
        static_cast<void>(rail);
        // parity: `RedStoneWireBlock.getStateForPlacement`, which draws the dust as it lands.
        settledStateId =
            world::redstoneWire::shapeOnPlacement(world->state(), placeAt.x, placeAt.y, placeAt.z, settledStateId)
                .value_or(settledStateId);
    }
    if (second.has_value()) {
        owner_->world().setBlockState(second->x, second->y, second->z, second->stateId);
    }
    // parity: `Level.setBlock` gives a block its block entity as it lands. Every block that owns one
    // needs it: a bell without one is a bell whose body the client never draws.
    if (world != nullptr && reg::blockEntityTypes::ofBlock(placedBlock) != nullptr) {
        server::world::Chunk* chunk = world->state().chunkIfLoaded(placeAt.x >> 4, placeAt.z >> 4);
        if (chunk != nullptr) {
            const std::optional<world::commandBlocks::Mode> mode = world::commandBlocks::modeOf(placedBlock.path());
            wire::nbt::NbtCompound data = world::blockEntityDefaults::of(placedBlock);
            // parity: `CommandBlock.setPlacedBy`, which is the placing half and not the command one:
            // a repeating block put down **by hand** comes out already automatic, and `TrackOutput`
            // starts as whatever `sendCommandFeedback` says. A `/setblock` runs none of this.
            if (mode.has_value()) {
                data.putBoolean("auto", *mode == world::commandBlocks::Mode::Repeating);
                data.putBoolean("TrackOutput", world->gameRules().get(world::gameRules::kSendCommandFeedback));
            }
            chunk->setBlockEntity(server::world::ChunkBlockEntity{placeAt.x & 15, placeAt.y, placeAt.z & 15,
                                                                  placedBlock, std::move(data)});
            if (mode.has_value()) {
                world::commandBlocks::onDataLoaded(world->state(), placeAt.x, placeAt.y, placeAt.z, false);
            }
        }
    }
    const Identifier* placedPath = reg::blockStateDefinitions::blockOf(blockStateId);
    // parity: `BlockItem.updateCustomBlockEntityTag`: a shulker box placed from an item that carries
    // contents gets them back, which is the other half of breaking one.
    if (world != nullptr && placedPath != nullptr && world::shulkers::isShulkerBox(placedPath->path())) {
        const wire::nbt::NbtCompound* carried = held->components.getCompound(world::shulkers::kContainer);
        server::world::Chunk* chunk = world->state().chunkIfLoaded(placeAt.x >> 4, placeAt.z >> 4);
        server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(placeAt.x & 15, placeAt.y, placeAt.z & 15);
        if (carried != nullptr && entity != nullptr) {
            world::shulkers::restore(entity->data(), *carried);
            chunk->markDirty();
            world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), placeAt.x, placeAt.y,
                                                                 placeAt.z,
                                                                 world::comparatorReading::of(world->state()));
        }
    }
    // parity: `MossyCarpetBlock.setPlacedBy`, which grows a second carpet over the one placed.
    if (world != nullptr && placedPath != nullptr && placedPath->path() == "pale_moss_carpet") {
        const std::optional<std::int32_t> topper =
            world::mossCarpetTopper::above(world->state(), placeAt.x, placeAt.y, placeAt.z,
                                           [this] { return options_.nextBool ? options_.nextBool() : false; });
        if (topper.has_value()) {
            owner_->world().setBlockState(placeAt.x, placeAt.y + 1, placeAt.z, *topper);
        }
    }

    if (session->gamemode == Gamemode::Survival) {
        session->inventory.set(heldSlot,
                               held->count <= 1 ? std::nullopt : std::optional{withCount(*held, held->count - 1)});
        owner_->sendInventorySlot(heldSlot);
    }

    for (const PlayerSession* player : owner_->playersInSameWorld()) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ClientboundBlockUpdatePacket{.pos = placeAt, .blockStateId = settledStateId}.write(buffer);
        });
    }
    if (second.has_value()) {
        broadcastBlockUpdate(BlockPos{second->x, second->y, second->z}, second->stateId);
    }
    // Both the block under it (a snow layer put on grass) and the block itself (grass put under
    // snow) can change how they look.
    refreshSnowUnder(placeAt);
    refreshSnowAt(placeAt);
    notifyNeighbours(placeAt);
    // parity: `GameEvent.BLOCK_PLACE`, thirteen: a block going down is louder than one coming up.
    heardBySculk("block_place", placeAt);
    if (world::signs::isSign(blockStateId)) {
        openNewSign(placeAt, blockStateId);
    }
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

    if (hooks_.bucketAllowed &&
        !hooks_.bucketAllowed(*session, static_cast<std::int32_t>(session->x), static_cast<std::int32_t>(session->y),
                              static_cast<std::int32_t>(session->z), item->toString(), item->path() == "bucket")) {
        owner_->sendInventory();
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const world::fluidFlow::Context context{.world = world->state(),
                                            .gameRules = world->gameRules(),
                                            .random = random,
                                            .ultrawarm =
                                                world->declaration().dimension == config::WorldDimension::Nether};
    const std::optional<world::buckets::Result> result =
        world::buckets::use(context, *item, session->x, session->y + world::fluids::eyeHeight(session->sneaking),
                            session->z, useItem.yaw, useItem.pitch, session->gamemode == Gamemode::Creative);
    if (!result.has_value()) {
        return;
    }
    // parity: `BucketItem` raises `FLUID_PICKUP` where it took the fluid from and `FLUID_PLACE`
    // where it put it, and a sculk sensor hears both. Which of the two it is comes from the item:
    // an empty bucket is the one that picks up.
    if (!result->changes.empty()) {
        const world::FluidChange& at = result->changes.front();
        if (world->state().hasSculkSensorNear(at.x, at.y, at.z, world::vibrations::kRange)) {
            world::vibrations::emit(world->state(), item->path() == "bucket" ? "fluid_pickup" : "fluid_place",
                                    at.x + 0.5, at.y + 0.5, at.z + 0.5, world->gameTime());
        }
    }
    for (const world::FluidChange& change : result->changes) {
        broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
    }
    // parity: `BucketItem.emptyContents` calls `setBlock`, so what was around it hears about it:
    // grass that had a snow layer over it until the water took its place stops wearing snow.
    for (const world::FluidChange& change : result->changes) {
        notifyNeighbours(BlockPos{change.x, change.y, change.z});
    }
    if (!result->replacement.has_value()) {
        return;
    }
    session->inventory.set(
        heldSlot,
        storage::StoredItemStack{.id = result->replacement->toString(), .count = 1, .components = held->components});
    owner_->sendInventorySlot(heldSlot);
}

} // namespace basalt::server::runtime
