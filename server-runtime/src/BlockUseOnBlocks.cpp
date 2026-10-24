#include "BlockUseOnShared.hpp"

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

#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
using namespace blockUseOn;

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
    if (const auto& protectedAt = owner_->options().spawnProtected;
        protectedAt && owner_->currentWorld() != nullptr &&
        protectedAt(*session, *owner_->currentWorld(), hit.pos.x, hit.pos.z)) {
        constexpr std::array<std::array<std::int32_t, 3>, 6> kStep{
            {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
        const std::array<std::int32_t, 3>& step = kStep[static_cast<std::size_t>(hit.direction)];
        for (const play::BlockPos& pos :
             {hit.pos, play::BlockPos{hit.pos.x + step[0], hit.pos.y + step[1], hit.pos.z + step[2]}}) {
            const std::optional<std::int32_t> actual = owner_->world().blockState(pos.x, pos.y, pos.z);
            owner_->sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
                play::ClientboundBlockUpdatePacket{.pos = pos, .blockStateId = actual.value_or(0)}.write(buffer);
            });
        }
        owner_->sendInventory();
        ack();
        return;
    }
    const bool mainHand = useItemOn.hand == play::Hand::MainHand;
    const std::int32_t heldSlot =
        mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    const std::optional<std::int32_t> clickedState = owner_->world().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    // parity: `DEFAULT_BLOCK_USE`.
    const auto blockUsed = [&] {
        awardBlockUse(*session, hit.pos, clickedState, std::nullopt, true);
        awardBlockUse(*session, hit.pos, clickedState, held, false);
        ack();
    };
    // The interact hook runs before the click does anything: chests, doors and placement are this one question.
    if (hooks_.interactAllowed &&
        !hooks_.interactAllowed(*session, hit.pos.x, hit.pos.y, hit.pos.z, held.has_value() ? held->id : "",
                                faceName(hit.direction), mainHand)) {
        owner_->sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ClientboundBlockUpdatePacket{.pos = hit.pos, .blockStateId = clickedState.value_or(0)}.write(buffer);
        });
        owner_->sendInventory();
        ack();
        return;
    }

    // parity: `ServerPlayerGameMode.useItemOn`.
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
        // parity: `RedStoneOreBlock.useItemOn`.
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
            // parity: `ChestBlock`.
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
        // parity: `CommandBlock.useWithoutItem`.
        // parity: `StructureBlockEntity.usedBy`.
        if (world::commandBlocks::modeOf(path).has_value() || path == "structure_block") {
            if (mayUseGameMasterBlocks(*session)) {
                sendBlockEntity(hit.pos);
                blockUsed();
                return;
            }
        }
        // parity: `RespawnAnchorBlock.useItemOn`.
        if (path == "respawn_anchor" && held.has_value() &&
            parsedOrNothing(held->id) == Identifier::vanilla("glowstone") &&
            chargeAnchor(*session, hit.pos, heldSlot, *held)) {
            blockUsed();
            return;
        }
        // parity: `JukeboxBlock.useItemOn`.
        if (path == "jukebox" && useJukebox(*session, hit.pos)) {
            blockUsed();
            return;
        }
        // parity: `LecternBlock.useItemOn`.
        if (path == "lectern" && useLectern(*session, hit.pos)) {
            blockUsed();
            return;
        }
        // parity: `ComposterBlock.useItemOn`.
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
        // parity: `CampfireBlock.useItemOn`.
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
        // parity: `AnvilBlock.useWithoutItem`.
        // parity: `EnchantingTableBlock.useWithoutItem`.
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
        // parity: `RepeaterBlock.useWithoutItem`.
        const std::optional<Identifier> inHand = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
        const bool axeInHand = inHand.has_value() && inHand->path().ends_with("_axe");
        const wire::common::Identifier* clickedBlock =
            clickedState.has_value() ? reg::blockStateDefinitions::blockOf(*clickedState) : nullptr;
        // parity: `WeatheringCopperGolemStatueBlock.useItemOn`.
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
    // parity: `TntBlock.useItemOn`.
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
    // parity: `BoneMealItem.useOn`.
    if (held.has_value() && heldBlock == Identifier::vanilla("bone_meal") &&
        useBoneMeal(*session, hit.pos, heldSlot, *held)) {
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `ItemFrameItem.useOn`.
    if (held.has_value() && heldBlock.has_value() &&
        (heldBlock->path().ends_with("item_frame") || heldBlock->path() == "painting") &&
        hangFrame(*session, hit, heldSlot, *held, *heldBlock)) {
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `HoneycombItem.useOn`.
    if (held.has_value() && heldBlock == Identifier::vanilla("honeycomb") &&
        useHoneycomb(*session, hit.pos, heldSlot, *held)) {
        awardItemUsedOn(*session, hit.pos, clickedState, held);
        player::statAwards::used(*session, held->id);
        ack();
        return;
    }
    // parity: `Item.useOn` over the `block_transformer` a hoe or a shovel carries.
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
    // parity: `ServerPlayerGameMode.useItemOn`.
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
    // parity: `CushionItem.useOn`.
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
    // parity: `EndCrystalItem.useOn`.
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
    // The client sends `use_item` only when nothing is in the way, so a bucket aimed at a block needs this.
    if (world::buckets::isBucket(*heldBlock)) {
        useBucketOn(*session, *heldBlock, heldSlot, *held, hit);
        ack();
        return;
    }

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

} // namespace basalt::server::runtime
