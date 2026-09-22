#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockExperience.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/SnowyBlocks.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using play::BlockPos;
using play::Gamemode;
using wire::common::Identifier;

/// parity: `Block.popResource` and a thrown stack's own opening movement.
constexpr double kPopJitter = 0.25;
constexpr double kPopSpread = 0.2;
constexpr double kItemHalfHeight = 0.125;
constexpr double kBreakDropLift = 0.2;
constexpr std::int32_t kBreakDropPickupDelay = 10;

[[nodiscard]] std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// parity: the two blocks `IceBlock` covers. Packed and blue ice are other blocks and do not melt.
[[nodiscard]] bool isMeltingIce(const std::int32_t stateId) {
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && (block->path() == "ice" || block->path() == "frosted_ice");
}

[[nodiscard]] std::int64_t seedFrom(const BreakContext& context) { return context.nextSeed ? context.nextSeed() : 0; }

[[nodiscard]] double doubleFrom(const BreakContext& context) { return context.nextDouble ? context.nextDouble() : 0.5; }

[[nodiscard]] double popJitter(const BreakContext& context) {
    return (doubleFrom(context) * 2 * kPopJitter) - kPopJitter;
}

/// What wears an item down, built from the two registries that decide it.
[[nodiscard]] item::ItemDamageEffects effectsFrom(const BreakContext& context) {
    return item::ItemDamageEffects{
        [&context](const Identifier& id) {
            return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        },
        [&context](const std::string_view tag, const Identifier& item) {
            return context.itemTags != nullptr && world::realItemTags::contains(*context.itemTags, tag, item);
        }};
}

void acknowledge(const BreakContext& context) {
    if (context.acknowledge) {
        context.acknowledge();
    }
}

void stopDigging(const BreakContext& context) {
    if (context.stopDigging) {
        context.stopDigging();
    }
}

} // namespace

void notifyAround(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                  const std::int32_t z, const std::optional<std::int32_t> removed) {
    if (context.blocks != nullptr) {
        context.blocks->notifyAround(world, playersIn(context, world.name()), x, y, z, removed);
    }
}

void reshapeAround(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                   const std::int32_t z) {
    if (context.blocks != nullptr) {
        context.blocks->reshapeAround(world, playersIn(context, world.name()), x, y, z);
    }
}

std::vector<PlayerSession*> playersIn(const BreakContext& context, const std::string_view worldName) {
    std::vector<PlayerSession*> found;
    if (context.registry == nullptr) {
        return found;
    }
    for (PlayerSession* player : context.registry->snapshot()) {
        if (player->worldName == worldName) {
            found.push_back(player);
        }
    }
    return found;
}

void announceBlock(const BreakContext& context, const std::string_view worldName, const std::int32_t x,
                   const std::int32_t y, const std::int32_t z, const std::int32_t stateId) {
    if (context.blocks != nullptr) {
        context.blocks->announceBlock(playersIn(context, worldName),
                                      world::BlockChange{.x = x, .y = y, .z = z, .stateId = stateId});
    }
}

void refreshSnowAt(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                   const std::int32_t z) {
    const std::optional<std::int32_t> updated = world::snowyBlocks::updated(world.state(), x, y, z);
    if (!updated.has_value() || !world.state().setBlockState(x, y, z, *updated)) {
        return;
    }
    announceBlock(context, world.name(), x, y, z, *updated);
}

void heardBySculk(const BreakContext& context, world::LoadedWorld& world, const std::string_view event,
                  const std::int32_t x, const std::int32_t y, const std::int32_t z,
                  const world::vibrations::Source& source) {
    std::vector<world::BlockChange> changes;
    world::vibrations::emit(world.state(), event, x + 0.5, y + 0.5, z + 0.5, world.gameTime(), source);
    for (const world::BlockChange& change : changes) {
        announceBlock(context, world.name(), change.x, change.y, change.z, change.stateId);
        for (const world::BlockChange& settled :
             world::redstone::settle(world.state(), world.name(), change.x, change.y, change.z)) {
            announceBlock(context, world.name(), settled.x, settled.y, settled.z, settled.stateId);
        }
    }
}

void spawnDroppedItem(const BreakContext& context, world::LoadedWorld& world, const Identifier& item,
                      const std::int32_t count, const double x, const double y, const double z, const double velocityX,
                      const double velocityY, const double velocityZ, const std::int32_t pickupDelay,
                      wire::nbt::NbtCompound components) {
    if (count <= 0 || reg::items::registry().byName(item) == nullptr) {
        return;
    }
    if (context.itemSpawnAllowed && !context.itemSpawnAllowed(world.name(), x, y, z, item.toString(), count, "drop")) {
        return;
    }
    if (context.registry == nullptr) {
        return;
    }
    // Only put into the world here. Telling anyone about it belongs to each connection's own
    // thread: broadcasting from here reaches whoever happens to be online at the instant of the
    // drop and nobody who walks up afterwards.
    static_cast<void>(world.state().spawnItem(world::DroppedItem{.entityId = context.registry->allocateEntityId(),
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

void dropLootFor(const BreakContext& context, world::LoadedWorld& world, const Identifier& block,
                 const std::int32_t stateId, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                 const wire::nbt::NbtCompound* carried, const storage::StoredItemStack* tool) {
    // parity: `Block.popResource` reads the `block_drops` rule, so with it off a block broken by hand
    // leaves nothing behind either.
    if (!world.gameRules().get(world::gameRules::kBlockDrops)) {
        return;
    }
    const server::world::loot::LootTable* table =
        context.lootTables == nullptr ? nullptr : context.lootTables->forBlock(block);
    if (table == nullptr) {
        return;
    }
    random::RandomSource scratch = random::RandomSource::legacy(seedFrom(context));
    // parity: the stream the table names, so the same world drops the same sapling off the same leaf.
    random::RandomSource& random =
        table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : scratch;
    // parity: `LootContextParams.BLOCK_STATE`: the lower half of a door is what carries the drop,
    // and `THIS_ENTITY`, which a block broken by hand always has.
    server::world::loot::LootContext lootContext{
        .blockState = reg::blockStateDefinitions::propertiesOf(stateId), .block = block, .hasEntity = true};
    // parity: `LootContextParams.TOOL`, which is what silk touch, shears and fortune are read from.
    if (tool != nullptr) {
        lootContext.tool = parsedOrNothing(tool->id);
        for (const auto& [enchantment, level] : enchantmentLevels(*tool)) {
            lootContext.toolEnchantments.emplace(enchantment.toString(), level);
        }
    } else {
        lootContext.tool = Identifier::vanilla("air");
    }
    const world::realItemTags::TagMembers* itemTags = context.itemTags;
    const world::realBlockTags::TagMembers* blockTags = context.blockTags;
    lootContext.tags = [itemTags, blockTags](std::string_view registry, const Identifier& id, std::string_view tag) {
        if (registry == "block") {
            return blockTags != nullptr && world::realBlockTags::contains(*blockTags, tag, id);
        }
        return registry == "item" && itemTags != nullptr && world::realItemTags::contains(*itemTags, tag, id);
    };
    context.lootTables->prepareAt(lootContext, world, x + 0.5, y + 0.5, z + 0.5);
    for (const server::world::loot::ItemStack& stack : table->generate(random, 0.0F, std::nullopt, lootContext)) {
        wire::nbt::NbtCompound components = stack.components;
        if (carried != nullptr && stack.item == block) {
            components.set(world::shulkers::kContainer, wire::nbt::NbtTag::ofCompound(*carried));
        }
        // parity: `Block.popResource`, which scatters the stack inside the block it came from and
        // pops it up, rather than dropping every stack on the same point.
        spawnDroppedItem(context, world, stack.item, stack.count, x + 0.5 + popJitter(context),
                         y + 0.5 + popJitter(context) - kItemHalfHeight, z + 0.5 + popJitter(context),
                         (doubleFrom(context) * kPopSpread) - (kPopSpread / 2.0), kBreakDropLift,
                         (doubleFrom(context) * kPopSpread) - (kPopSpread / 2.0), kBreakDropPickupDelay,
                         std::move(components));
    }
}

std::optional<std::int32_t> meltedIce(const BreakContext& context, world::LoadedWorld& world,
                                      const std::optional<std::int32_t> broken, const std::int32_t x,
                                      const std::int32_t y, const std::int32_t z) {
    // Silk touch is not read yet, so ice always melts here; the Nether evaporates it as vanilla does.
    if (!broken.has_value() || !isMeltingIce(*broken)) {
        return std::nullopt;
    }
    if (config::dimensionFacts(world.declaration().dimension).hasCeiling) {
        return std::nullopt;
    }
    // parity: 26.3 `IceBlock.playerDestroy`: `ice_melts_when_destroyed_above` or a liquid block below.
    const std::optional<std::int32_t> below = world.state().blockState(x, y - 1, z);
    const Identifier* belowBlock = below.has_value() ? reg::blockStateDefinitions::blockOf(*below) : nullptr;
    if (belowBlock == nullptr) {
        return std::nullopt;
    }
    const bool liquid = belowBlock->path() == "water" || belowBlock->path() == "lava";
    const bool tagged =
        context.blockTags != nullptr &&
        world::realBlockTags::contains(*context.blockTags, "minecraft:ice_melts_when_destroyed_above", *belowBlock);
    if (!liquid && !tagged) {
        return std::nullopt;
    }
    return reg::blocks::water().stateId;
}

void wearHeldItemOnBreak(const BreakContext& context, PlayerSession& player, const Identifier& block) {
    PlayerSession* session = &player;
    if (session->gamemode == Gamemode::Creative) {
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
    wearHeldItem(context, player, item::itemWear::costPerBlock(*held));
}

void wearHeldItem(const BreakContext& context, PlayerSession& player, const std::int32_t cost) {
    PlayerSession* session = &player;
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    if (session->gamemode == Gamemode::Creative || !held.has_value() || cost <= 0) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(seedFrom(context));
    const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
        *held, cost, random,
        [&context](const storage::StoredItemStack& stack, const std::int32_t amount, random::RandomSource& source) {
            return effectsFrom(context).process(stack, amount, source);
        });
    if (worn == held) {
        return;
    }
    if (context.itemDamageAllowed && !context.itemDamageAllowed(*session, held->id, cost, !worn.has_value())) {
        return;
    }
    session->inventory.set(heldSlot, worn);
    if (context.slotChanged) {
        context.slotChanged(*session, heldSlot);
    }
}

bool breakBlockFor(PlayerSession& player, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                   const std::int32_t z, const BreakContext& context) {
    const std::optional<std::int32_t> broken = world.state().blockState(x, y, z);
    const Identifier* brokenBlock = broken.has_value() ? reg::blockStateDefinitions::blockOf(*broken) : nullptr;
    PlayerSession* session = &player;
    const BlockPos pos{x, y, z};
    // Cancelling puts the block back on the digger's screen: the client broke it locally the moment
    // it started, so saying nothing would leave a hole only they can see.
    if (context.breakAllowed && !context.breakAllowed(*session, x, y, z, broken.value_or(0))) {
        acknowledge(context);
        if (context.putBack) {
            context.putBack(*session, x, y, z, broken.value_or(0));
        }
        stopDigging(context);
        return false;
    }
    // parity: `ShulkerBoxBlock.getDrops`, read before the block goes: breaking it takes the block
    // entity with it, and the contents have to travel in the item.
    std::optional<wire::nbt::NbtCompound> carried;
    if (brokenBlock != nullptr && world::shulkers::isShulkerBox(brokenBlock->path())) {
        server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        if (entity != nullptr) {
            carried = world::shulkers::contentsOf(entity->data());
        }
    }
    // parity: `TripWireBlock.playerWillDestroy`, which writes `disarmed` before the string goes and
    // raises `GameEvent.SHEAR`. It is what makes shears the way to cut a trap without setting it off:
    // the line is handed a disarmed string, so it comes loose instead of firing.
    std::optional<std::int32_t> removed = broken;
    if (broken.has_value() && world::tripwires::isWire(*broken)) {
        const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
        const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
        if (held.has_value() && held->id == Identifier::vanilla("shears").toString()) {
            if (const std::optional<std::int32_t> disarmed = reg::blockStateDefinitions::stateIdOf(
                    world::tripwires::wireBlock(), {{"disarmed", "true"}}, *broken);
                disarmed.has_value()) {
                static_cast<void>(world.state().setBlockState(x, y, z, *disarmed));
                removed = *disarmed;
                heardBySculk(context, world, "shear", x, y, z, {.entity = player.uuid()});
            }
        }
    }
    // parity: `TntBlock.playerWillDestroy`: an `unstable` block broken by a survival player lights
    // instead of dropping. `unstable` is read nowhere else, and a signal lights a block whatever it
    // says, so this is the only place it means anything.
    if (session->gamemode != Gamemode::Creative && broken.has_value() && brokenBlock != nullptr &&
        brokenBlock->path() == "tnt" && world.gameRules().get(world::gameRules::kTntExplodes) &&
        reg::blockStateDefinitions::propertiesOf(*broken).contains("unstable") &&
        reg::blockStateDefinitions::propertiesOf(*broken).at("unstable") == "true") {
        // parity: `prime` only spawns the entity and raises the event; taking the block away and
        // dropping whatever it drops is the ordinary break that follows, exactly as in the jar.
        world.state().primeTnt(x, y, z, "prime_fuse");
    }
    // parity: `DoublePlantBlock.playerWillDestroy` drops the half before either goes, so its
    // `location_check` still finds the other half; `playerDestroy` then drops nothing.
    const bool dropsBeforeRemoval = session->gamemode == Gamemode::Survival && broken.has_value() &&
                                    brokenBlock != nullptr && reg::blockStateFamilies::isDoublePlantState(*broken);
    if (dropsBeforeRemoval) {
        const std::optional<storage::StoredItemStack> held = session->inventory.heldItem();
        dropLootFor(context, world, *brokenBlock, *broken, x, y, z, nullptr, held.has_value() ? &*held : nullptr);
    }
    const std::int32_t leaves = meltedIce(context, world, broken, x, y, z).value_or(reg::blocks::air().stateId);
    const bool changed = world.state().setBlockState(x, y, z, leaves);
    acknowledge(context);
    if (!changed) {
        return false;
    }
    if (brokenBlock != nullptr) {
        session->stats.increment(storage::stats::mined(brokenBlock->toString()));
    }

    const std::vector<PlayerSession*> watchers = playersIn(context, world.name());
    for (const PlayerSession* watcher : watchers) {
        sendBlockChangeTo(*watcher, x, y, z, leaves);
    }
    // parity: `Level.levelEvent(player, 2001, pos, state)`: the breaker predicts its own puff of
    // particles and its sound, everyone else needs telling.
    if (broken.has_value()) {
        const play::LevelEventPacket effect{
            .event = play::LevelEventPacket::kParticlesDestroyBlock, .pos = pos, .data = *broken};
        for (const PlayerSession* other : watchers) {
            if (other->entityId() == session->entityId()) {
                continue;
            }
            if (other->connection().sendLevelEvent) {
                other->connection().sendLevelEvent(play::LevelEventPacket::kParticlesDestroyBlock, x, y, z, *broken);
                continue;
            }
            if (other->connection().sendPlayPacket) {
                other->connection().sendPlayPacket(play::LevelEventPacket::kId,
                                                   [&effect](wire::io::ByteBuffer& buffer) { effect.write(buffer); });
            }
        }
    }
    stopDigging(context);
    refreshSnowAt(context, world, x, y - 1, z);
    notifyAround(context, world, x, y, z, removed);
    // parity: `GameEvent.BLOCK_DESTROY`, which a sculk sensor hears as twelve.
    heardBySculk(context, world, "block_destroy", x, y, z, {.entity = player.uuid()});
    // Breaking the frame puts the portal out, which is the other half of lighting one.
    for (const std::array<std::int32_t, 3>& block : world::netherPortals::extinguishAround(world, x, y, z)) {
        announceBlock(context, world.name(), block[0], block[1], block[2], reg::blocks::air().stateId);
    }

    if (session->gamemode == Gamemode::Survival && brokenBlock != nullptr && !dropsBeforeRemoval) {
        // parity: `Player.hasCorrectToolForDrops`. A block marked as needing the right tool gives
        // nothing to the wrong one, which is what makes punching stone a waste of time.
        const std::optional<storage::StoredItemStack> held = session->inventory.heldItem();
        const std::optional<Identifier> heldItem = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
        const reg::BlockHardness* hardness = reg::blocks::hardnessOf(*brokenBlock);
        const world::realBlockTags::TagMembers* tags = context.blockTags;
        const bool correctTool = hardness == nullptr || !hardness->requiresCorrectTool ||
                                 reg::itemTools::isCorrectForDrops(
                                     heldItem.has_value() ? &*heldItem : nullptr, *brokenBlock,
                                     [tags](std::string_view tag, const Identifier& block) {
                                         return tags != nullptr && world::realBlockTags::contains(*tags, tag, block);
                                     });
        if (correctTool) {
            dropLootFor(context, world, *brokenBlock, broken.value_or(0), x, y, z,
                        carried.has_value() ? &*carried : nullptr, held.has_value() ? &*held : nullptr);
            // parity: `spawnAfterBreak(..., true)` right after the drops, and `popExperience` reads
            // `block_drops` like the items do.
            const bool silkTouch = held.has_value() && enchantmentLevel(*held, Identifier::vanilla("silk_touch")) > 0;
            random::RandomSource random = random::RandomSource::legacy(seedFrom(context));
            const std::int32_t points = world::blockExperience::onBreak(*brokenBlock, silkTouch, random);
            if (points > 0 && context.awardExperience && world.gameRules().get(world::gameRules::kBlockDrops)) {
                context.awardExperience(world, x + 0.5, y + 0.5, z + 0.5, points);
            }
        }
    }
    if (brokenBlock != nullptr) {
        wearHeldItemOnBreak(context, *session, *brokenBlock);
    }
    return true;
}

} // namespace basalt::server::runtime::player
