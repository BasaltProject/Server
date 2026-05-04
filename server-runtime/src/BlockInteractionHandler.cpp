#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerDigSpeed.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>
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

/// parity: `Block.popResource` and a thrown stack's own opening movement.
constexpr double kPopJitter = 0.25;
constexpr double kTossLiftSpread = 0.1;
constexpr double kTossSpread = 0.02;
constexpr double kThrowSpeed = 0.3;
constexpr double kThrowLift = 0.1;
constexpr double kHandBelowEyes = static_cast<double>(0.3F);

/// parity: `ClientboundPlayerActionPacket`'s own off-hand number.

std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// parity: the `attack_animation` every item carries.
play::SwingAnimation attackAnimationOf(std::string_view itemId) {
    static constexpr std::array<std::pair<std::string_view, std::int32_t>, 7> kSpears = {{
        {"minecraft:wooden_spear", 13},
        {"minecraft:stone_spear", 15},
        {"minecraft:copper_spear", 17},
        {"minecraft:iron_spear", 19},
        {"minecraft:golden_spear", 19},
        {"minecraft:diamond_spear", 21},
        {"minecraft:netherite_spear", 23},
    }};
    for (const auto& [name, duration] : kSpears) {
        if (name == itemId) {
            return play::SwingAnimation{.type = play::SwingAnimationType::Stab, .duration = duration};
        }
    }
    return play::SwingAnimation{};
}

} // namespace

BlockInteractionHandler::BlockInteractionHandler(ClientSession& owner, Hooks hooks, Options options)
    : owner_(&owner), hooks_(std::move(hooks)), options_(std::move(options)),
      damageEffects_(
          [&owner](const Identifier& id) {
              world::RealEnchantments* enchantments = owner.options().enchantments;
              return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
          },
          [&owner](std::string_view tag, const Identifier& item) {
              const world::realItemTags::TagMembers* tags = owner.options().itemTags;
              return tags != nullptr && world::realItemTags::contains(*tags, tag, item);
          }) {}

std::int64_t BlockInteractionHandler::nextSeed() const { return options_.nextSeed ? options_.nextSeed() : 0; }

double BlockInteractionHandler::nextDouble() const { return options_.nextDouble ? options_.nextDouble() : 0.5; }

double BlockInteractionHandler::popJitter() const { return (nextDouble() * 2 * kPopJitter) - kPopJitter; }

double BlockInteractionHandler::tossLift() const { return (nextDouble() - nextDouble()) * kTossLiftSpread; }

bool BlockInteractionHandler::isAirLike(std::int32_t stateId) const { return reg::blocks::isAir(stateId); }

bool BlockInteractionHandler::eyesInWater() const {
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return false;
    }
    const double eyeY = session->y + world::fluids::eyeHeight(session->sneaking);
    const std::optional<std::int32_t> state = owner_->world().blockState(
        static_cast<std::int32_t>(std::floor(session->x)), static_cast<std::int32_t>(std::floor(eyeY)),
        static_cast<std::int32_t>(std::floor(session->z)));
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

float BlockInteractionHandler::destroyProgressPerTick(const Identifier& block,
                                                      const reg::BlockHardness* hardness) const {
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return 0.0F;
    }
    return player::destroyProgressPerTickFor(*session, block, hardness, owner_->options().blockTags, eyesInWater());
}

bool BlockInteractionHandler::breaksInOneTick(const Identifier* block, const reg::BlockHardness* hardness) const {
    if (block == nullptr || hardness == nullptr || hardness->isUnbreakable()) {
        return false;
    }
    return hardness->isInstant() || destroyProgressPerTick(*block, hardness) >= 1.0F;
}

void BlockInteractionHandler::broadcastBlockUpdate(const BlockPos& pos, std::int32_t stateId) {
    BlockChangeBroadcaster* blocks = owner_->options().blocks;
    if (blocks != nullptr) {
        blocks->announceBlock(owner_->playersInSameWorld(),
                              world::BlockChange{.x = pos.x, .y = pos.y, .z = pos.z, .stateId = stateId});
    }
}

void BlockInteractionHandler::broadcastEffects(const BlockPos& pos, std::int32_t stateId,
                                               const std::vector<world::BlockEffect>& effects) {
    BlockChangeBroadcaster* blocks = owner_->options().blocks;
    if (blocks != nullptr) {
        blocks->sendEffects(owner_->playersInSameWorld(), pos.x, pos.y, pos.z, stateId, effects);
    }
}

void BlockInteractionHandler::notifyNeighbours(const BlockPos& pos) {
    BlockChangeBroadcaster* blocks = owner_->options().blocks;
    world::LoadedWorld* world = owner_->currentWorld();
    if (blocks != nullptr && world != nullptr) {
        blocks->notifyAround(*world, owner_->playersInSameWorld(), pos.x, pos.y, pos.z, std::nullopt);
    }
}

void BlockInteractionHandler::notifyNeighboursAfterRemoval(const BlockPos& pos, std::int32_t removedStateId) {
    BlockChangeBroadcaster* blocks = owner_->options().blocks;
    world::LoadedWorld* world = owner_->currentWorld();
    if (blocks != nullptr && world != nullptr) {
        blocks->notifyAround(*world, owner_->playersInSameWorld(), pos.x, pos.y, pos.z, removedStateId);
    }
}

player::BreakContext BlockInteractionHandler::breakContext() {
    player::BreakContext context;
    context.blocks = owner_->options().blocks;
    context.registry = owner_->options().playerRegistry;
    context.lootTables = owner_->options().lootTables;
    context.blockTags = owner_->options().blockTags;
    context.enchantments = owner_->options().enchantments;
    context.itemTags = owner_->options().itemTags;
    context.triggers = owner_->options().advancementTriggers;
    context.nextDouble = options_.nextDouble;
    context.nextSeed = options_.nextSeed;
    context.minecartImprovements = [this] {
        if (!owner_->options().enabledFeatures) {
            return false;
        }
        const std::vector<std::string> features = owner_->options().enabledFeatures();
        return std::ranges::contains(features, std::string{"minecraft:minecart_improvements"});
    };
    context.breakAllowed = hooks_.breakAllowed;
    context.itemSpawnAllowed = hooks_.itemSpawnAllowed;
    context.itemDamageAllowed = hooks_.itemDamageAllowed;
    context.putBack = [this](const PlayerSession&, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                             const std::int32_t stateId) {
        owner_->sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ClientboundBlockUpdatePacket{.pos = BlockPos{x, y, z}, .blockStateId = stateId}.write(buffer);
        });
    };
    context.slotChanged = [this](const PlayerSession&, const std::int32_t slot) { owner_->sendInventorySlot(slot); };
    context.stopDigging = [this] { stopDigging(); };
    context.dismount = [this](player::PlayerSession& player) { owner_->dismount(player); };
    context.creakingHeartBroken = [this](world::LoadedWorld& world, const PlayerSession& player, const std::int32_t x,
                                         const std::int32_t y, const std::int32_t z) {
        if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr) {
            mobs->creakingHeartBroken(world, player, {x, y, z});
        }
    };
    context.guardedBroken = [this](world::LoadedWorld& world, const PlayerSession& player, const std::int32_t stateId) {
        MobActionSystem* mobs = owner_->options().mobs;
        const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
        if (mobs != nullptr && block != nullptr &&
            mobs->blockTagged("minecraft:guarded_by_piglins", *block).value_or(false)) {
            mobs->angerNearbyPiglins(world, player, false);
        }
    };
    context.hiveBroken = [this](world::LoadedWorld& world, const PlayerSession& player, const std::int32_t x,
                                const std::int32_t y, const std::int32_t z, const std::int32_t stateId,
                                const wire::nbt::NbtCompound& hive, const bool silkTouch) {
        if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr) {
            mobs->hiveBroken(world, player, {x, y, z}, stateId, hive, silkTouch);
        }
    };
    context.awardExperience = owner_->options().awardExperience;
    return context;
}

void BlockInteractionHandler::heardBySculk(std::string_view event, const BlockPos& pos) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        const PlayerSession* session = owner_->playerSession();
        player::heardBySculk(breakContext(), *world, event, pos.x, pos.y, pos.z,
                             {.entity = session != nullptr ? std::optional{session->uuid()} : std::nullopt});
    }
}

void BlockInteractionHandler::spawnDroppedItem(const Identifier& item, std::int32_t count, double x, double y, double z,
                                               double velocityX, double velocityY, double velocityZ,
                                               std::int32_t pickupDelay, wire::nbt::NbtCompound components,
                                               std::optional<wire::common::Uuid> thrower) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::spawnDroppedItem(breakContext(), *world, item, count, x, y, z, velocityX, velocityY, velocityZ,
                                 pickupDelay, std::move(components), thrower);
    }
}

void BlockInteractionHandler::dropLootFor(const Identifier& block, std::int32_t stateId, const BlockPos& pos,
                                          const wire::nbt::NbtCompound* carried) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::dropLootFor(breakContext(), *world, block, stateId, pos.x, pos.y, pos.z, carried);
    }
}

void BlockInteractionHandler::wearHeldItem(std::int32_t cost) {
    if (PlayerSession* session = owner_->playerSession(); session != nullptr) {
        player::wearHeldItem(breakContext(), *session, cost);
    }
}

void BlockInteractionHandler::wearHeldItemOnBreak(const Identifier& block) {
    PlayerSession* session = owner_->playerSession();
    if (session != nullptr) {
        player::wearHeldItemOnBreak(breakContext(), *session, block);
    }
}

void BlockInteractionHandler::dropHeldItem(bool all) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || session->gamemode == Gamemode::Spectator) {
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    if (!held.has_value()) {
        return;
    }
    const std::int32_t dropCount = all ? held->count : 1;
    if (hooks_.dropAllowed && !hooks_.dropAllowed(*session, held->id, dropCount)) {
        owner_->sendInventorySlot(heldSlot);
        return;
    }
    const std::int32_t remaining = held->count - dropCount;
    storage::StoredItemStack left = *held;
    left.count = remaining;
    session->inventory.set(heldSlot, remaining <= 0 ? std::nullopt : std::optional{left});
    owner_->sendInventorySlot(heldSlot);

    const std::optional<Identifier> item = parsedOrNothing(held->id);
    if (!item.has_value()) {
        return;
    }
    // parity: `Player.drop`.
    const double yaw = static_cast<double>(session->yaw) * std::numbers::pi / 180.0;
    const double pitch = static_cast<double>(session->pitch) * std::numbers::pi / 180.0;
    const double tossAngle = nextDouble() * 2 * std::numbers::pi;
    const double tossSpread = kTossSpread * nextDouble();
    // parity: `createItemStackToDrop`.
    spawnDroppedItem(*item, dropCount, session->x,
                     session->y + world::fluids::eyeHeight(session->sneaking) - kHandBelowEyes, session->z,
                     (-std::sin(yaw) * std::cos(pitch) * kThrowSpeed) + (std::cos(tossAngle) * tossSpread),
                     (-std::sin(pitch) * kThrowSpeed) + kThrowLift + tossLift(),
                     (std::cos(yaw) * std::cos(pitch) * kThrowSpeed) + (std::sin(tossAngle) * tossSpread),
                     world::kDefaultPickupDelay, held->components, session->uuid());
    player::statAwards::awardThrown(*session, held->id, dropCount);
}

void BlockInteractionHandler::handlePunch() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    // parity: `handlePunch` resets the attack strength ticker along with the swing.
    session->ticksSinceSwing = 0;
    const std::optional<storage::StoredItemStack>& held = session->inventory.heldItem();
    const play::SwingAnimationPacket packet{
        .entityId = session->entityId(), .hand = 0, .animation = attackAnimationOf(held.has_value() ? held->id : "")};
    // The swinging client animates its own arm, so this goes to the others.
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session->entityId()) {
            continue;
        }
        if (other->connection().sendSwing) {
            other->connection().sendSwing(session->entityId(), false);
            continue;
        }
        if (other->connection().sendPlayPacket) {
            other->connection().sendPlayPacket(play::SwingAnimationPacket::kId,
                                               [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void BlockInteractionHandler::startDigging(const BlockPos& pos) {
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr || (session->gamemode != Gamemode::Survival && session->gamemode != Gamemode::Adventure)) {
        return;
    }
    if (hooks_.digAllowed && !hooks_.digAllowed(*session, pos.x, pos.y, pos.z)) {
        return;
    }
    diggingAt_ = pos;
    diggingTicks_ = 0;
    diggingStage_ = -1;
}

void BlockInteractionHandler::stopDigging() {
    if (!diggingAt_.has_value()) {
        return;
    }
    const BlockPos pos = *diggingAt_;
    diggingAt_.reset();
    diggingStage_ = -1;
    broadcastDestruction(pos, play::BlockDestructionPacket::kNone, 0.0F);
}

void BlockInteractionHandler::tickDigging(PlayerSession& session) {
    static_cast<void>(session);
    if (!diggingAt_.has_value()) {
        return;
    }
    const BlockPos pos = *diggingAt_;
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> state =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    const Identifier* block = state.has_value() ? reg::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr || reg::blocks::isAir(*state)) {
        stopDigging();
        return;
    }
    diggingTicks_++;
    // parity: `ServerPlayerGameMode.tick`.
    constexpr std::int32_t kDestroyDust = 2019;
    constexpr std::int32_t kDestroyDustAndSound = 2020;
    constexpr double kLevelEventRange = 64.0;
    const std::int32_t event = diggingTicks_ % 4 == 0 ? kDestroyDustAndSound : kDestroyDust;
    for (const PlayerSession* watcher : owner_->playersInSameWorld()) {
        const double dx = watcher->x - (pos.x + 0.5);
        const double dy = watcher->y - (pos.y + 0.5);
        const double dz = watcher->z - (pos.z + 0.5);
        if (watcher->connection().sendLevelEvent &&
            (dx * dx) + (dy * dy) + (dz * dz) < kLevelEventRange * kLevelEventRange) {
            watcher->connection().sendLevelEvent(event, pos.x, pos.y, pos.z, diggingDirection_);
        }
    }
    const float perTick = destroyProgressPerTick(*block, reg::blocks::hardnessOf(*block));
    if (perTick <= 0.0F) {
        return;
    }
    const auto stage = static_cast<std::int32_t>(static_cast<float>(diggingTicks_) * perTick *
                                                 static_cast<float>(play::BlockDestructionPacket::kStages));
    if (stage >= play::BlockDestructionPacket::kStages || stage == diggingStage_) {
        return;
    }
    diggingStage_ = stage;
    broadcastDestruction(pos, stage, perTick);
}

void BlockInteractionHandler::broadcastDestruction(const BlockPos& pos, std::int32_t stage, float perTick) {
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session->entityId() || !other->connection().sendBlockDestruction) {
            continue;
        }
        other->connection().sendBlockDestruction(session->entityId(), pos.x, pos.y, pos.z, stage, perTick);
    }
}

void BlockInteractionHandler::handlePlayerAction(const play::ServerboundPlayerActionPacket& action) {
    using Action = play::ServerboundPlayerActionPacket::Action;

    const auto ack = [this, &action] {
        owner_->sendCompressed(play::BlockChangedAckPacket::kId, [&action](wire::io::ByteBuffer& buffer) {
            play::BlockChangedAckPacket{.sequence = action.sequence}.write(buffer);
        });
    };

    switch (action.action) {
    case Action::DropItem:
        dropHeldItem(false);
        break;
    case Action::DropAllItems:
        dropHeldItem(true);
        break;
    case Action::ReleaseUseItem:
        // parity: letting go before the animation ends eats nothing at all.
        owner_->itemUseTimers().stopEating();
        owner_->itemUseTimers().releaseBow();
        owner_->itemUseTimers().stopBlocking();
        owner_->itemUseTimers().stopSpyglass();
        owner_->combat().stopCharge();
        break;
    case Action::SwapItemWithOffhand:
        owner_->swapWithOffhand();
        break;
    case Action::Stab:
        owner_->combat().jab();
        break;
    default:
        break;
    }

    // The cracks everyone else sees.
    if (action.action == Action::StartDestroyBlock) {
        // parity: `NoteBlock.attack`, which plays the note without moving it on.
        if (world::LoadedWorld* loaded = owner_->currentWorld(); loaded != nullptr) {
            const std::vector<world::BlockEffect> struck =
                world::noteBlocks::strike(loaded->state(), action.pos.x, action.pos.y, action.pos.z);
            if (!struck.empty()) {
                broadcastEffects(action.pos,
                                 owner_->world().blockState(action.pos.x, action.pos.y, action.pos.z).value_or(0),
                                 struck);
            }
            // parity: `NoteBlock.attack` counts the strike even when the block above muffles it.
            const std::optional<std::int32_t> struckState =
                loaded->state().blockState(action.pos.x, action.pos.y, action.pos.z);
            const Identifier* struckBlock =
                struckState.has_value() ? reg::blockStateDefinitions::blockOf(*struckState) : nullptr;
            if (PlayerSession* striker = owner_->playerSession();
                striker != nullptr && struckBlock != nullptr && struckBlock->path() == "note_block") {
                player::statAwards::award(*striker, "play_noteblock");
            }
            // parity: `RedStoneOreBlock.attack`.
            if (const std::optional<world::BlockChange> glow =
                    world::miscGrowth::lightRedstoneOre(loaded->state(), action.pos.x, action.pos.y, action.pos.z);
                glow.has_value()) {
                broadcastBlockUpdate(action.pos, glow->stateId);
            }
        }
        startDigging(action.pos);
        diggingDirection_ = static_cast<std::int32_t>(action.direction);
    } else if (action.action == Action::ChangeDestroyDirection) {
        // parity: 26.3 `handleBlockBreakAction`, which only turns the face the dust flies off.
        diggingDirection_ = static_cast<std::int32_t>(action.direction);
    } else if (action.action == Action::AbortDestroyBlock || action.action == Action::StopDestroyBlock) {
        stopDigging();
    }

    PlayerSession* session = owner_->playerSession();
    const Gamemode gamemode = session == nullptr ? owner_->options().defaultGamemode : session->gamemode;
    const std::optional<std::int32_t> targetState =
        owner_->world().blockState(action.pos.x, action.pos.y, action.pos.z);
    const Identifier* targetBlock =
        targetState.has_value() ? reg::blockStateDefinitions::blockOf(*targetState) : nullptr;
    const reg::BlockHardness* targetHardness = targetBlock == nullptr ? nullptr : reg::blocks::hardnessOf(*targetBlock);
    std::optional<Action> breaksOn;
    if (gamemode == Gamemode::Creative) {
        breaksOn = Action::StartDestroyBlock;
    } else if (gamemode == Gamemode::Survival) {
        // A one-tick break is destroyed by the client at dig start with no end sent; waiting for `Stop` broke flowers.
        breaksOn = breaksInOneTick(targetBlock, targetHardness) ? Action::StartDestroyBlock : Action::StopDestroyBlock;
    }
    if (gamemode == Gamemode::Survival && targetHardness != nullptr && targetHardness->isUnbreakable()) {
        ack();
        return;
    }
    if (!breaksOn.has_value() || action.action != *breaksOn) {
        ack();
        return;
    }

    // The block simply goes, shared with players that have no `ClientSession`; the clock and answer stay here.
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr) {
        ack();
        return;
    }
    player::BreakContext context = breakContext();
    context.acknowledge = ack;
    static_cast<void>(player::breakBlockFor(*session, *world, action.pos.x, action.pos.y, action.pos.z, context));
}

} // namespace basalt::server::runtime
