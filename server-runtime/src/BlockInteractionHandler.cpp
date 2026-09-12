#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
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
constexpr double kPopSpread = 0.2;
constexpr double kItemHalfHeight = 0.125;
constexpr double kBreakDropLift = 0.2;
constexpr std::int32_t kBreakDropPickupDelay = 10;
constexpr double kTossLiftSpread = 0.1;
constexpr double kTossSpread = 0.02;
constexpr double kThrowSpeed = 0.3;
constexpr double kThrowLift = 0.1;
constexpr double kThrowEyeHeight = 1.5;

/// parity: `ClientboundPlayerActionPacket`'s own off-hand number.
constexpr std::int32_t kOffhand = 1;

std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// parity: the two blocks `IceBlock` covers. Packed and blue ice are other blocks and do not melt.
[[nodiscard]] bool isMeltingIce(std::int32_t stateId) {
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && (block->path() == "ice" || block->path() == "frosted_ice");
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
    if (hardness == nullptr || hardness->isUnbreakable()) {
        return 0.0F;
    }
    const PlayerSession* session = owner_->playerSession();
    const std::optional<storage::StoredItemStack> held =
        session == nullptr ? std::nullopt : session->inventory.heldItem();
    const std::optional<Identifier> heldItem = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const auto amplifierOf = [session](const Identifier& effect) {
        if (session == nullptr) {
            return -1;
        }
        const auto found = session->effects.find(effect);
        return found == session->effects.end() ? -1 : found->second.amplifier;
    };
    const reg::itemTools::DigConditions conditions{
        .efficiencyLevel = held.has_value() ? player::enchantmentLevel(*held, Identifier::vanilla("efficiency")) : 0,
        .hasteAmplifier = amplifierOf(world::mobEffects::haste()),
        .miningFatigueAmplifier = amplifierOf(world::mobEffects::miningFatigue()),
        .underwater = eyesInWater(),
        .inAir = session != nullptr && !session->onGround};
    const world::realBlockTags::TagMembers* tags = owner_->options().blockTags;
    return reg::itemTools::destroyProgressPerTick(
        heldItem.has_value() ? &*heldItem : nullptr, block, hardness->destroyTime, hardness->requiresCorrectTool,
        conditions, [tags](std::string_view tag, const Identifier& tagged) {
            return tags != nullptr && world::realBlockTags::contains(*tags, tag, tagged);
        });
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

void BlockInteractionHandler::heardBySculk(std::string_view event, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    std::vector<world::BlockChange> changes;
    world::vibrations::emit(world->state(), event, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, world->gameTime());
    for (const world::BlockChange& change : changes) {
        broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
        for (const world::BlockChange& settled :
             world::redstone::settle(world->state(), world->name(), change.x, change.y, change.z)) {
            broadcastBlockUpdate(BlockPos{settled.x, settled.y, settled.z}, settled.stateId);
        }
    }
}

void BlockInteractionHandler::spawnDroppedItem(const Identifier& item, std::int32_t count, double x, double y, double z,
                                               double velocityX, double velocityY, double velocityZ,
                                               std::int32_t pickupDelay, wire::nbt::NbtCompound components) {
    if (count <= 0 || reg::items::registry().byName(item) == nullptr) {
        return;
    }
    if (hooks_.itemSpawnAllowed &&
        !hooks_.itemSpawnAllowed(owner_->worldName(), x, y, z, item.toString(), count, "drop")) {
        return;
    }
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (registry == nullptr) {
        return;
    }
    // Only put into the world here. Telling anyone about it belongs to each connection's own
    // thread: broadcasting from here reaches whoever happens to be online at the instant of the
    // drop and nobody who walks up afterwards.
    static_cast<void>(owner_->world().spawnItem(world::DroppedItem{.entityId = registry->allocateEntityId(),
                                                                   .uuid = wire::common::Uuid::random(),
                                                                   .item = item,
                                                                   .count = count,
                                                                   .x = x,
                                                                   .y = y,
                                                                   .z = z,
                                                                   .velocityX = velocityX,
                                                                   .velocityY = velocityY,
                                                                   .velocityZ = velocityZ,
                                                                   .pickupDelay = pickupDelay,
                                                                   .components = std::move(components)}));
}

void BlockInteractionHandler::dropLootFor(const Identifier& block, std::int32_t stateId, const BlockPos& pos,
                                          const wire::nbt::NbtCompound* carried) {
    // parity: `Block.popResource` reads the `block_drops` rule, so with it off a block broken by hand
    // leaves nothing behind either.
    world::LoadedWorld* world = owner_->currentWorld();
    if (world != nullptr && !world->gameRules().get(world::gameRules::kBlockDrops)) {
        return;
    }
    world::RealLootTables* tables = owner_->options().lootTables;
    const server::world::loot::LootTable* table = tables == nullptr ? nullptr : tables->forBlock(block);
    if (table == nullptr) {
        return;
    }
    random::RandomSource scratch = random::RandomSource::legacy(nextSeed());
    // parity: the stream the table names, so the same world drops the same sapling off the same leaf.
    random::RandomSource& random = table->randomSequence.has_value() && world != nullptr
                                       ? world->randomSequences().of(*table->randomSequence)
                                       : scratch;
    // parity: `LootContextParams.BLOCK_STATE`: the lower half of a door is what carries the drop,
    // and `THIS_ENTITY`, which a block broken by hand always has.
    const server::world::loot::LootContext context{.blockState = reg::blockStateDefinitions::propertiesOf(stateId),
                                                   .hasEntity = true};
    for (const server::world::loot::ItemStack& stack : table->generate(random, 0.0F, std::nullopt, context)) {
        wire::nbt::NbtCompound components;
        if (carried != nullptr && stack.item == block) {
            components.set(world::shulkers::kContainer, wire::nbt::NbtTag::ofCompound(*carried));
        }
        // parity: `Block.popResource`, which scatters the stack inside the block it came from and
        // pops it up, rather than dropping every stack on the same point.
        spawnDroppedItem(
            stack.item, stack.count, pos.x + 0.5 + popJitter(), pos.y + 0.5 + popJitter() - kItemHalfHeight,
            pos.z + 0.5 + popJitter(), (nextDouble() * kPopSpread) - (kPopSpread / 2.0), kBreakDropLift,
            (nextDouble() * kPopSpread) - (kPopSpread / 2.0), kBreakDropPickupDelay, std::move(components));
    }
}

std::optional<std::int32_t> BlockInteractionHandler::meltedIce(std::optional<std::int32_t> broken,
                                                               const BlockPos& pos) const {
    // Silk touch is not read yet, so ice always melts here; the Nether evaporates it as vanilla does.
    if (!broken.has_value() || !isMeltingIce(*broken)) {
        return std::nullopt;
    }
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || config::dimensionFacts(world->declaration().dimension).hasCeiling) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> below = world->state().blockState(pos.x, pos.y - 1, pos.z);
    if (!below.has_value() || !server::world::heightmapMatches(server::world::HeightmapType::MotionBlocking, *below)) {
        return std::nullopt;
    }
    return reg::blocks::water().stateId;
}

void BlockInteractionHandler::wearHeldItemOnBreak(const Identifier& block) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || session->gamemode == Gamemode::Creative) {
        return;
    }
    const reg::BlockHardness* hardness = reg::blocks::hardnessOf(block);
    // parity: `Item.mineBlock` wears nothing on a block that comes apart on its own.
    if (hardness == nullptr || hardness->isInstant()) {
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    if (!held.has_value()) {
        return;
    }
    const std::int32_t cost = item::itemWear::costPerBlock(*held);
    if (cost <= 0) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
        *held, cost, random,
        [this](const storage::StoredItemStack& stack, std::int32_t amount, random::RandomSource& source) {
            return damageEffects_.process(stack, amount, source);
        });
    if (worn == held) {
        return;
    }
    if (hooks_.itemDamageAllowed && !hooks_.itemDamageAllowed(*session, held->id, cost, !worn.has_value())) {
        return;
    }
    session->inventory.set(heldSlot, worn);
    owner_->sendInventorySlot(heldSlot);
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
    if (hooks_.dropAllowed && !hooks_.dropAllowed(*session, held->id)) {
        // The client has already taken it out of its own hand.
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
    // parity: `Player.drop` throws it forward from eye height along the look direction, rather than
    // dropping it on the player's own feet where they would instantly walk back into it.
    const double yaw = static_cast<double>(session->yaw) * std::numbers::pi / 180.0;
    const double pitch = static_cast<double>(session->pitch) * std::numbers::pi / 180.0;
    const double tossAngle = nextDouble() * 2 * std::numbers::pi;
    const double tossSpread = kTossSpread * nextDouble();
    spawnDroppedItem(*item, dropCount, session->x, session->y + kThrowEyeHeight, session->z,
                     (-std::sin(yaw) * std::cos(pitch) * kThrowSpeed) + (std::cos(tossAngle) * tossSpread),
                     (-std::sin(pitch) * kThrowSpeed) + kThrowLift + tossLift(),
                     (std::cos(yaw) * std::cos(pitch) * kThrowSpeed) + (std::sin(tossAngle) * tossSpread));
}

void BlockInteractionHandler::handleSwing(std::int32_t hand) {
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    // The swinging client animates its own arm, which is why this goes to the others and not back.
    const play::AnimatePacket packet{.entityId = session->entityId(),
                                     .action = hand == kOffhand ? play::AnimatePacket::kSwingOffHand
                                                                : play::AnimatePacket::kSwingMainHand};
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session->entityId() || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::AnimatePacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
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
    broadcastDestruction(pos, play::BlockDestructionPacket::kNone);
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
    // The same progress the break itself uses, so the cracks run at the speed the tool really mines
    // at rather than a guess.
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
    broadcastDestruction(pos, stage);
}

void BlockInteractionHandler::broadcastDestruction(const BlockPos& pos, std::int32_t stage) {
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    const play::BlockDestructionPacket packet{
        .entityId = session->entityId(), .pos = pos, .stage = static_cast<std::uint8_t>(stage)};
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session->entityId() || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::BlockDestructionPacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
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
        break;
    case Action::SwapItemWithOffhand:
        owner_->swapWithOffhand();
        break;
    default:
        break;
    }

    // The cracks everyone else sees. The digger draws its own, which is why this never goes back to
    // them. parity: `ServerPlayerGameMode.destroyAndAck` and its `destroyProgress`.
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
            // parity: `RedStoneOreBlock.attack` runs the same `interact` a right click does, so an
            // ore lights whichever button was pressed on it.
            if (const std::optional<world::BlockChange> glow =
                    world::miscGrowth::lightRedstoneOre(loaded->state(), action.pos.x, action.pos.y, action.pos.z);
                glow.has_value()) {
                broadcastBlockUpdate(action.pos, glow->stateId);
            }
        }
        startDigging(action.pos);
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
        // A block that comes apart in one tick is destroyed by the client the moment the dig starts,
        // and it never sends the end of one: waiting for `Stop` there is why flowers, grass and
        // leaves could not be broken in survival at all.
        breaksOn = breaksInOneTick(targetBlock, targetHardness) ? Action::StartDestroyBlock : Action::StopDestroyBlock;
    }
    // Bedrock and the portal frames stay put for anyone who is not in creative.
    if (gamemode == Gamemode::Survival && targetHardness != nullptr && targetHardness->isUnbreakable()) {
        ack();
        return;
    }
    if (!breaksOn.has_value() || action.action != *breaksOn) {
        ack();
        return;
    }

    const std::optional<std::int32_t> broken = targetState;
    // Cancelling puts the block back on the digger's screen: the client broke it locally the moment
    // it started, so saying nothing would leave a hole only they can see.
    if (session != nullptr && hooks_.breakAllowed &&
        !hooks_.breakAllowed(*session, action.pos.x, action.pos.y, action.pos.z, broken.value_or(0))) {
        ack();
        owner_->sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ClientboundBlockUpdatePacket{.pos = action.pos, .blockStateId = broken.value_or(0)}.write(buffer);
        });
        stopDigging();
        return;
    }
    // parity: `ShulkerBoxBlock.getDrops`, read before the block goes: breaking it takes the block
    // entity with it, and the contents have to travel in the item.
    std::optional<wire::nbt::NbtCompound> carried;
    world::LoadedWorld* world = owner_->currentWorld();
    const Identifier* brokenBlock = broken.has_value() ? reg::blockStateDefinitions::blockOf(*broken) : nullptr;
    if (world != nullptr && brokenBlock != nullptr && world::shulkers::isShulkerBox(brokenBlock->path())) {
        server::world::Chunk* chunk = world->state().chunkIfLoaded(action.pos.x >> 4, action.pos.z >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(action.pos.x & 15, action.pos.y, action.pos.z & 15);
        if (entity != nullptr) {
            carried = world::shulkers::contentsOf(entity->data());
        }
    }
    // parity: `TripWireBlock.playerWillDestroy`, which writes `disarmed` before the string goes and
    // raises `GameEvent.SHEAR`. It is what makes shears the way to cut a trap without setting it off:
    // the line is handed a disarmed string, so it comes loose instead of firing.
    std::optional<std::int32_t> removed = broken;
    if (session != nullptr && broken.has_value() && world::tripwires::isWire(*broken)) {
        const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
        const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
        if (held.has_value() && held->id == Identifier::vanilla("shears").toString()) {
            if (const std::optional<std::int32_t> disarmed = reg::blockStateDefinitions::stateIdOf(
                    world::tripwires::wireBlock(), {{"disarmed", "true"}}, *broken);
                disarmed.has_value()) {
                static_cast<void>(owner_->world().setBlockState(action.pos.x, action.pos.y, action.pos.z, *disarmed));
                removed = *disarmed;
                heardBySculk("shear", action.pos);
            }
        }
    }
    // parity: `TntBlock.playerWillDestroy`: an `unstable` block broken by a survival player lights
    // instead of dropping. `unstable` is read nowhere else, and a signal lights a block whatever it
    // says, so this is the only place it means anything.
    if (session != nullptr && session->gamemode != Gamemode::Creative && broken.has_value() &&
        brokenBlock != nullptr && brokenBlock->path() == "tnt" && world != nullptr &&
        world->gameRules().get(world::gameRules::kTntExplodes) &&
        reg::blockStateDefinitions::propertiesOf(*broken).contains("unstable") &&
        reg::blockStateDefinitions::propertiesOf(*broken).at("unstable") == "true") {
        // parity: `prime` only spawns the entity and raises the event; taking the block away and
        // dropping whatever it drops is the ordinary break that follows, exactly as in the jar.
        world->state().primeTnt(action.pos.x, action.pos.y, action.pos.z, "prime_fuse");
    }
    const std::int32_t leaves = meltedIce(broken, action.pos).value_or(reg::blocks::air().stateId);
    const bool changed = owner_->world().setBlockState(action.pos.x, action.pos.y, action.pos.z, leaves);
    ack();
    if (!changed) {
        return;
    }
    if (brokenBlock != nullptr && session != nullptr) {
        session->stats.increment(storage::stats::mined(brokenBlock->toString()));
    }

    const std::vector<PlayerSession*> watchers = owner_->playersInSameWorld();
    for (const PlayerSession* player : watchers) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(play::ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ClientboundBlockUpdatePacket{.pos = action.pos, .blockStateId = leaves}.write(buffer);
        });
    }
    // parity: `Level.levelEvent(player, 2001, pos, state)`: the breaker predicts its own puff of
    // particles and its sound, everyone else needs telling.
    if (broken.has_value()) {
        const play::LevelEventPacket effect{
            .event = play::LevelEventPacket::kParticlesDestroyBlock, .pos = action.pos, .data = *broken};
        for (const PlayerSession* other : watchers) {
            if ((session != nullptr && other->entityId() == session->entityId()) ||
                !other->connection().sendPlayPacket) {
                continue;
            }
            other->connection().sendPlayPacket(play::LevelEventPacket::kId,
                                               [&effect](wire::io::ByteBuffer& buffer) { effect.write(buffer); });
        }
    }
    stopDigging();
    refreshSnowUnder(action.pos);
    if (removed.has_value()) {
        notifyNeighboursAfterRemoval(action.pos, *removed);
    } else {
        notifyNeighbours(action.pos);
    }
    // parity: `GameEvent.BLOCK_DESTROY`, which a sculk sensor hears as twelve.
    heardBySculk("block_destroy", action.pos);
    if (world != nullptr) {
        // Breaking the frame puts the portal out, which is the other half of lighting one.
        for (const std::array<std::int32_t, 3>& block :
             world::netherPortals::extinguishAround(*world, action.pos.x, action.pos.y, action.pos.z)) {
            broadcastBlockUpdate(BlockPos{block[0], block[1], block[2]}, reg::blocks::air().stateId);
        }
    }

    if (gamemode == Gamemode::Survival && brokenBlock != nullptr) {
        // parity: `Player.hasCorrectToolForDrops`. A block marked as needing the right tool gives
        // nothing to the wrong one, which is what makes punching stone a waste of time.
        const std::optional<storage::StoredItemStack> held =
            session == nullptr ? std::nullopt : session->inventory.heldItem();
        const std::optional<Identifier> heldItem = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
        const reg::BlockHardness* hardness = reg::blocks::hardnessOf(*brokenBlock);
        const world::realBlockTags::TagMembers* tags = owner_->options().blockTags;
        const bool correctTool = hardness == nullptr || !hardness->requiresCorrectTool ||
                                 reg::itemTools::isCorrectForDrops(
                                     heldItem.has_value() ? &*heldItem : nullptr, *brokenBlock,
                                     [tags](std::string_view tag, const Identifier& block) {
                                         return tags != nullptr && world::realBlockTags::contains(*tags, tag, block);
                                     });
        if (correctTool) {
            dropLootFor(*brokenBlock, broken.value_or(0), action.pos, carried.has_value() ? &*carried : nullptr);
        }
    }
    if (brokenBlock != nullptr) {
        wearHeldItemOnBreak(*brokenBlock);
    }
}

} // namespace basalt::server::runtime
