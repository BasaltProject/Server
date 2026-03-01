#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockItemUse.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Waxing.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <optional>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

using wire::protocol::play::Gamemode;

namespace reg = server::registry;

/// parity: `BoneMealItem.addGrowthParticles`, fifteen of them on a plain crop.
constexpr std::int32_t kGrowthSparkles = 15;

[[nodiscard]] std::int64_t seedOf(const BreakContext& context) { return context.nextSeed ? context.nextSeed() : 0; }

[[nodiscard]] item::ItemDamageEffects effectsOf(const BreakContext& context) {
    return item::ItemDamageEffects{
        [&context](const wire::common::Identifier& id) {
            return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        },
        [&context](const std::string_view tag, const wire::common::Identifier& item) {
            return context.itemTags != nullptr && world::realItemTags::contains(*context.itemTags, tag, item);
        }};
}

/// One off the stack in hand, and the slot said out loud. Creative spends nothing.
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

/// A block written by hand: the state, the particles it draws, the neighbours and the sensor.
void reshapeBlock(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                  const std::int32_t z, const std::int32_t stateId, const std::int32_t effect) {
    world.state().setBlockState(x, y, z, stateId);
    world::BlockChange change{.x = x, .y = y, .z = z, .stateId = stateId};
    if (effect != 0) {
        change.effects.emplace_back(world::blockEffect::Effect{.type = effect, .data = 0});
    }
    if (context.blocks != nullptr) {
        context.blocks->announceBlock(playersIn(context, world.name()), change);
    }
    // parity: `setBlock(pos, state, 11)`, which does tell the neighbours, and `GameEvent.BLOCK_CHANGE`.
    notifyAround(context, world, x, y, z, std::nullopt);
    heardBySculk(context, world, "block_change", x, y, z);
}

/// The item that id names, or nothing when it is not one this build knows how to read.
[[nodiscard]] std::optional<wire::common::Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = wire::common::Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// parity: `Block.popResource`, which scatters what it pops a quarter of a block either way.
constexpr double kPopJitter = 0.25;

[[nodiscard]] double popJitter(const BreakContext& context) {
    const double roll = context.nextDouble ? context.nextDouble() : 0.5;
    return (roll * 2 * kPopJitter) - kPopJitter;
}

/// The comparators beside a block whose reading just changed.
void notifyComparators(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z,
                                                         world::comparatorReading::of(world.state()));
}

/// One point off whatever is in hand, and the slot said out loud when it changed.
void wearOne(const BreakContext& context, PlayerSession& player, const std::int32_t heldSlot,
             const storage::StoredItemStack& held) {
    random::RandomSource wear = random::RandomSource::legacy(seedOf(context));
    const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
        held, 1, wear,
        [&context](const storage::StoredItemStack& stack, const std::int32_t amount, random::RandomSource& source) {
            return effectsOf(context).process(stack, amount, source);
        });
    if (worn == held) {
        return;
    }
    player.inventory.set(heldSlot, worn);
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
}

} // namespace

bool useBoneMealFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
                    const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                    const storage::StoredItemStack& held) {
    const std::optional<std::int32_t> existing = world.state().blockState(x, y, z);
    // parity: `BoneMealItem.growCrop` spends the item on a valid target whether or not the growth
    // roll did anything, and answers no on anything else so the click falls through.
    if (!existing.has_value() || !world::boneMeal::grows(*existing)) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(seedOf(context));
    std::optional<world::BlockChange> grown = world::boneMeal::apply(world.state(), x, y, z, random);
    if (grown.has_value()) {
        grown->effects.emplace_back(
            world::blockEffect::Effect{.type = world::blockEffect::Effect::kPlantGrowth, .data = kGrowthSparkles});
        if (context.blocks != nullptr) {
            context.blocks->announceBlock(playersIn(context, world.name()), *grown);
        }
    }
    spendOne(context, player, heldSlot, held);
    return true;
}

bool useHoneycombFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                     const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                     const storage::StoredItemStack& held) {
    const std::optional<std::int32_t> existing = world.state().blockState(x, y, z);
    const std::optional<std::int32_t> waxed =
        existing.has_value() ? world::waxing::waxed(*existing) : std::optional<std::int32_t>{};
    if (!waxed.has_value()) {
        return false;
    }
    reshapeBlock(context, world, x, y, z, *waxed, world::blockEffect::Effect::kWaxOn);
    spendOne(context, player, heldSlot, held);
    return true;
}

void primeTntFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
                 const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                 const storage::StoredItemStack& held, const bool wears) {
    // parity: `TntBlock.prime` answers false with the `tnt_explodes` rule off, and then nothing at
    // all happens: the block stays and the item is not spent.
    if (!world.gameRules().get(world::gameRules::kTntExplodes)) {
        return;
    }
    if (!world.state().setBlockState(x, y, z, reg::blocks::air().stateId)) {
        return;
    }
    announceBlock(context, world.name(), x, y, z, reg::blocks::air().stateId);
    // parity: `TntBlock.prime` raises `PRIME_FUSE`, which is what a sculk sensor hears.
    world.state().primeTnt(x, y, z, "prime_fuse");
    notifyAround(context, world, x, y, z, std::nullopt);
    if (player.gamemode == Gamemode::Creative) {
        return;
    }
    if (wears) {
        wearOne(context, player, heldSlot, held);
        return;
    }
    // parity: a fire charge is consumed rather than worn.
    spendOne(context, player, heldSlot, held);
}

void igniteFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
               const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
               const storage::StoredItemStack& held) {
    const std::optional<std::int32_t> existing = world.state().blockState(x, y, z);
    if (!existing.has_value() || !server::registry::blocks::isAir(*existing)) {
        return;
    }
    // parity: `BaseFireBlock.onPlace` checks the frame, not the item, which is why any fire in the
    // right place opens a portal and why the portal replaces the fire rather than burning next to it.
    if (const std::optional<server::world::portal::PortalShape> shape = world::netherPortals::light(world, x, y, z);
        shape.has_value()) {
        const std::int32_t portal = server::world::portal::PortalShape::portalState(shape->axis());
        for (const server::world::portal::PortalBlock& block : shape->interior()) {
            announceBlock(context, world.name(), block.x, block.y, block.z, portal);
        }
    } else {
        if (!world::netherPortals::placeFire(world, x, y, z)) {
            return;
        }
        const std::optional<std::int32_t> fire = world.state().blockState(x, y, z);
        announceBlock(context, world.name(), x, y, z, fire.value_or(reg::blocks::air().stateId));
        // parity: `BaseFireBlock.onPlace`, which is what starts fire acting at all.
        random::RandomSource random = random::RandomSource::legacy(seedOf(context));
        world::fires::schedule(world.state(), x, y, z, random);
    }
    if (player.gamemode == Gamemode::Creative) {
        return;
    }
    wearOne(context, player, heldSlot, held);
}

bool useAxeFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
               const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
               const storage::StoredItemStack& held) {
    const std::optional<std::int32_t> existing = world.state().blockState(x, y, z);
    const std::optional<world::waxing::Scrape> cut =
        existing.has_value() ? world::waxing::scraped(*existing) : std::optional<world::waxing::Scrape>{};
    if (!cut.has_value()) {
        return false;
    }
    // parity: stripping a log is only a sound, and the other two draw their own particles.
    const std::int32_t effect = cut->cut == world::waxing::Cut::Scraped   ? world::blockEffect::Effect::kCopperScrape
                                : cut->cut == world::waxing::Cut::Unwaxed ? world::blockEffect::Effect::kWaxOff
                                                                          : 0;
    reshapeBlock(context, world, x, y, z, cut->stateId, effect);
    if (player.gamemode == Gamemode::Creative) {
        return true;
    }
    // parity: `hurtAndBreak(1, ...)`, one point off the axe whichever of the three it did.
    wearOne(context, player, heldSlot, held);
    return true;
}

bool useComposterFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                     const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held) {
    const std::optional<std::int32_t> level = world::composters::levelAt(world.state(), x, y, z);
    if (!level.has_value() || context.blocks == nullptr) {
        return false;
    }
    // parity: `useWithoutItem` on a ready composter hands out the bone meal whatever is in the hand,
    // and `useItemOn` only looks at the item while there is room. Both end in a level change, which
    // is what the comparator beside it reads.
    if (*level == world::composters::kReady) {
        const std::optional<world::BlockChange> emptied = world::composters::empty(world.state(), x, y, z);
        if (!emptied.has_value()) {
            return false;
        }
        announceBlock(context, world.name(), x, y, z, emptied->stateId);
        // parity: `extractProduce` pops the bone meal a hair above the block and a little to a side.
        spawnDroppedItem(context, world, wire::common::Identifier::vanilla("bone_meal"), 1,
                         x + 0.5 + popJitter(context), y + 1.01, z + 0.5 + popJitter(context), 0.0, 0.0, 0.0, 0,
                         wire::nbt::NbtCompound{});
        const std::vector<world::BlockEffect> noise{
            world::blockEffect::Sound{.name = "block.composter.empty",
                                      .source = wire::protocol::play::SoundSource::Blocks,
                                      .volume = 1.0F,
                                      .pitch = 1.0F}};
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, emptied->stateId, noise);
        notifyComparators(world, x, y, z);
        return true;
    }

    const std::optional<wire::common::Identifier> item =
        held.has_value() ? parsedOrNothing(held->id) : std::optional<wire::common::Identifier>{};
    if (!item.has_value() || !world::composters::chanceOf(*item).has_value() || *level >= world::composters::kFull) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(seedOf(context));
    const std::optional<world::BlockChange> filled = world::composters::addItem(world.state(), x, y, z, *item, random);
    // parity: `levelEvent(1500, pos, changed ? 1 : 0)`, which is what makes the client play the
    // success sound and the sparkles rather than the plain fill.
    const std::vector<world::BlockEffect> fill{world::blockEffect::Effect{
        .type = world::blockEffect::Effect::kComposterFill, .data = filled.has_value() ? 1 : 0}};
    context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, filled.has_value() ? filled->stateId : 0,
                                fill);
    if (filled.has_value()) {
        announceBlock(context, world.name(), x, y, z, filled->stateId);
        notifyComparators(world, x, y, z);
    }
    spendOne(context, player, heldSlot, *held);
    return true;
}

bool placeOnCampfireFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                        const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                        const std::optional<storage::StoredItemStack>& held, world::RealRecipes* recipes) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const std::optional<wire::common::Identifier> item =
        held.has_value() ? parsedOrNothing(held->id) : std::optional<wire::common::Identifier>{};
    if (!stateId.has_value() || !world::campfires::isLitCampfire(*stateId) || !item.has_value()) {
        return false;
    }
    server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return false;
    }
    server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr) {
        chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, world::campfires::block(), {}});
        entity = chunk->blockEntityAt(x & 15, y, z & 15);
        if (entity == nullptr) {
            return false;
        }
    }
    world::campfires::State state = world::campfires::read(entity->data());
    const bool placed = world::campfires::placeFood(
        state, server::world::loot::ItemStack{.item = *item, .count = 1},
        [recipes](const server::world::loot::ItemStack& food) {
            return recipes == nullptr ? nullptr : recipes->cookingFor(server::world::crafting::Cooker::Campfire, food);
        });
    if (!placed) {
        return false;
    }
    world::campfires::write(state, entity->data());
    chunk->markDirty();
    notifyComparators(world, x, y, z);
    spendOne(context, player, heldSlot, *held);
    return true;
}

} // namespace basalt::server::runtime::player
