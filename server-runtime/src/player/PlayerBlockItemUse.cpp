#include <basalt/server/plugins/PluginHookEventTypes.hpp>
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
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/BlockTransformers.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Waxing.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>
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
    // parity: `setBlock(pos, state, 11)`.
    notifyAround(context, world, x, y, z, std::nullopt);
    heardBySculk(context, world, "block_change", x, y, z);
}

[[nodiscard]] std::optional<wire::common::Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = wire::common::Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// parity: `Block.popResource`.
constexpr double kPopJitter = 0.25;

[[nodiscard]] double popJitter(const BreakContext& context) {
    const double roll = context.nextDouble ? context.nextDouble() : 0.5;
    return (roll * 2 * kPopJitter) - kPopJitter;
}

/// parity: `Block.dropFromBlockInteractLootTable` into the transform's `drop_strategy`.
void dropTransformLoot(const BreakContext& context, world::LoadedWorld& world, const wire::common::Identifier& table,
                       const std::int32_t stateId, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                       const wire::protocol::play::Direction face, const bool fromFace,
                       const storage::StoredItemStack& held) {
    const server::world::loot::LootTable* rolled =
        context.lootTables == nullptr ? nullptr : context.lootTables->byId(table);
    if (rolled == nullptr || !world.gameRules().get(world::gameRules::kBlockDrops)) {
        return;
    }
    random::RandomSource scratch = random::RandomSource::legacy(seedOf(context));
    random::RandomSource& random =
        rolled->randomSequence.has_value() ? world.randomSequences().of(*rolled->randomSequence) : scratch;
    server::world::loot::LootContext loot{.blockState = reg::blockStateDefinitions::propertiesOf(stateId),
                                          .block = reg::blockStateDefinitions::blockOf(stateId) == nullptr
                                                       ? std::nullopt
                                                       : std::optional{*reg::blockStateDefinitions::blockOf(stateId)},
                                          .hasEntity = true,
                                          .tool = parsedOrNothing(held.id)};
    context.lootTables->prepareAt(loot, world, x + 0.5, y + 0.5, z + 0.5);
    const std::int32_t stepX = wire::protocol::play::normalX(face);
    const std::int32_t stepY = wire::protocol::play::normalY(face);
    const std::int32_t stepZ = wire::protocol::play::normalZ(face);
    const auto between = [&context](double least, double most) {
        return least + ((context.nextDouble ? context.nextDouble() : 0.5) * (most - least));
    };
    for (const server::world::loot::ItemStack& stack : rolled->generate(random, 0.0F, std::nullopt, loot)) {
        constexpr double kHalf = 0.125;
        // parity: `Block.popResource`.
        if (!fromFace) {
            spawnDroppedItem(context, world, stack.item, stack.count, x + 0.5 + popJitter(context),
                             y + 0.5 + popJitter(context) - kHalf, z + 0.5 + popJitter(context), between(-0.1, 0.1),
                             0.2, between(-0.1, 0.1), 10, stack.components);
            continue;
        }
        // parity: `Block.popResourceFromFace`, just outside the face that was clicked.
        const double px = x + 0.5 + (stepX == 0 ? between(-0.25, 0.25) : stepX * (0.5 + kHalf));
        const double py = y + 0.5 + (stepY == 0 ? between(-0.25, 0.25) : stepY * (0.5 + kHalf)) - kHalf;
        const double pz = z + 0.5 + (stepZ == 0 ? between(-0.25, 0.25) : stepZ * (0.5 + kHalf));
        const double vx = stepX == 0 ? between(-0.1, 0.1) : stepX * 0.1;
        const double vy = stepY == 0 ? between(0.0, 0.1) : (stepY * 0.1) + 0.1;
        const double vz = stepZ == 0 ? between(-0.1, 0.1) : stepZ * 0.1;
        spawnDroppedItem(context, world, stack.item, stack.count, px, py, pz, vx, vy, vz, 10, stack.components);
    }
}

void notifyComparators(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z,
                                                         world::comparatorReading::of(world.state()));
}

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
    world::triggerFacts::durabilityChanged(context.triggers, player, held, worn);
    player.inventory.set(heldSlot, worn);
    if (!worn.has_value()) {
        statAwards::itemBroken(player, held.id,
                               heldSlot == PlayerInventory::kOffhandSlot ? statAwards::kBreakOffHand
                                                                         : statAwards::kBreakMainHand,
                               context.registry);
    }
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
}

} // namespace

bool useBoneMealFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t x,
                    const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                    const storage::StoredItemStack& held) {
    const std::optional<std::int32_t> existing = world.state().blockState(x, y, z);
    // parity: `BoneMealItem.growCrop` spends the item on a valid target whether or not growth did anything.
    if (!existing.has_value() || !world::boneMeal::grows(world.state(), *existing)) {
        return false;
    }
    if (context.fertilizeAllowed && !context.fertilizeAllowed(player, x, y, z, *existing)) {
        spendOne(context, player, heldSlot, held);
        return true;
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
    // parity: `TntBlock.prime` answers false with `tnt_explodes` off.
    if (!world.gameRules().get(world::gameRules::kTntExplodes) ||
        !world.state().tntMayPrime(x, y, z, "item", player.name())) {
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
    // parity: `BaseFireBlock.onPlace` checks the frame.
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

bool transformBlockFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                       world::RealBlockTransformers& transformers, const wire::common::Identifier& transformer,
                       const std::int32_t x, const std::int32_t y, const std::int32_t z,
                       const wire::protocol::play::Direction face, const std::int32_t /*heldSlot*/,
                       const storage::StoredItemStack& held) {
    // parity: `playerHasBlockingItemUseIntent`.
    const std::optional<storage::StoredItemStack>& offhand = player.inventory.at(PlayerInventory::kOffhandSlot);
    if (offhand.has_value() && offhand->id == "minecraft:shield" && !player.sneaking) {
        return false;
    }
    const world::RealBlockTransformers::Tagged tagged = [&context](std::string_view tag,
                                                                   const wire::common::Identifier& block) {
        return context.blockTags != nullptr && world::realBlockTags::contains(*context.blockTags, tag, block);
    };
    const std::optional<std::int32_t> was = world.state().blockState(x, y, z);
    if (!was.has_value()) {
        return false;
    }
    for (const world::RealBlockTransformers::Transform& transform : transformers.of(transformer)) {
        if (std::ranges::find(transform.disallowedFaces, face) != transform.disallowedFaces.end()) {
            continue;
        }
        const std::optional<std::int32_t> turned =
            world::RealBlockTransformers::stateFor(transform, world.state(), x, y, z, tagged);
        if (!turned.has_value()) {
            continue;
        }
        if (transform.loot.has_value()) {
            dropTransformLoot(context, world, *transform.loot, *was, x, y, z, face,
                              transform.drop == world::RealBlockTransformers::Drop::ClickedFace, held);
        }
        if (player.gamemode != Gamemode::Creative && transform.itemDamagePerUse > 0) {
            wearHeldItem(context, player, transform.itemDamagePerUse);
        }
        const std::int32_t effect =
            transform.particle == world::RealBlockTransformers::Particle::Scrape
                ? world::blockEffect::Effect::kCopperScrape
            : transform.particle == world::RealBlockTransformers::Particle::WaxOff ? world::blockEffect::Effect::kWaxOff
            : transform.particle == world::RealBlockTransformers::Particle::WaxOn  ? world::blockEffect::Effect::kWaxOn
                                                                                   : 0;
        reshapeBlock(context, world, x, y, z, *turned, effect);
        // parity: `level.playSound(player, ...)`.
        if (!transform.sound.empty() && context.blocks != nullptr) {
            std::vector<PlayerSession*> others = playersIn(context, world.name());
            std::erase(others, &player);
            const std::string_view sound = std::string_view{transform.sound}.starts_with("minecraft:")
                                               ? std::string_view{transform.sound}.substr(10)
                                               : std::string_view{transform.sound};
            context.blocks->sendEffects(others, x, y, z, *turned,
                                        {world::blockEffect::Sound{.name = std::string{sound}}});
        }
        return true;
    }
    return false;
}

bool useComposterFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                     const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held) {
    const std::optional<std::int32_t> level = world::composters::levelAt(world.state(), x, y, z);
    if (!level.has_value() || context.blocks == nullptr) {
        return false;
    }
    // parity: `useWithoutItem`.
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
    const std::int32_t chance = world::composters::chanceOf(*item).value_or(0);
    if (!world.events().allowed(&server::plugins::pluginEventTypes::blockCompost,
                                [&](server::plugins::PluginEvent& event) {
                                    plugins::EventSink::player(event, player);
                                    event.set("x", static_cast<std::int64_t>(x));
                                    event.set("y", static_cast<std::int64_t>(y));
                                    event.set("z", static_cast<std::int64_t>(z));
                                    event.set("item", item->toString());
                                    event.set("raises", *level == 0 || chance >= 100);
                                })) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(seedOf(context));
    const std::optional<world::BlockChange> filled = world::composters::addItem(world.state(), x, y, z, *item, random);
    // parity: `levelEvent(1500, pos, changed ? 1 : 0)` makes the client play the success sound and sparkles.
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
    // parity: `CampfireBlock.useItemOn`, the statistic before the item is spent.
    statAwards::award(player, "interact_with_campfire");
    spendOne(context, player, heldSlot, *held);
    return true;
}

} // namespace basalt::server::runtime::player
