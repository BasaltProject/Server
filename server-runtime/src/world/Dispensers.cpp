#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RespawnAnchors.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/Waxing.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::dispensers {
namespace {

using registry::BlockFace;
using ItemStack = server::world::loot::ItemStack;

/// parity: `DispenserBlock`'s four ticks of delay.
constexpr std::int32_t kDelay = 4;

/// parity: `DefaultDispenseItemBehavior.spawnItem`, the shove it gives what it throws.
constexpr double kSpeedLow = 0.2;
constexpr double kSpeedSpread = 0.1;
constexpr double kThrowLift = 0.2;

/// parity: `DefaultDispenseItemBehavior.DEFAULT_ACCURACY` and the spread one point of it is worth.
constexpr double kSpreadPerAccuracy = 0.0172275;
constexpr double kDefaultAccuracy = 6.0;

/// parity: `DispenserBlock.getDispensePosition`'s own 0.7 out of the face, and the drop below the
/// middle that is smaller out of the top and the bottom.
constexpr double kDispenseReach = 0.7;

/// parity: `BoatDispenseItemBehavior`'s own `0.5625` clear of the block, plus the `1.125` it rises by
/// when the dispenser looks up or down.
/// The length of `_spawn_egg`, which is what a spawn egg's name ends with.
constexpr std::size_t kSpawnEggSuffix = 10;

constexpr double kBoatClearance = 0.5625;
constexpr double kBoatLift = 1.125;
constexpr double kVerticalDrop = 0.125;
constexpr double kSidewaysDrop = 0.15625;

/// parity: the `15` vanilla passes with `LevelEvent.PARTICLES_AND_SOUND_PLANT_GROWTH`.
constexpr std::int32_t kGrowthSparkles = 15;

[[nodiscard]] const wire::common::Identifier& tnt() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("tnt");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& carvedPumpkin() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("carved_pumpkin");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& brush() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("brush");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& boneMealItem() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("bone_meal");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& shears() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("shears");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& glassBottle() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("glass_bottle");
    return instance;
}

/// parity: `PotionContents.createItemStack(Items.POTION, Potions.WATER)`, which is a plain potion
/// carrying one component.
[[nodiscard]] ItemStack waterBottle() {
    wire::nbt::NbtCompound contents;
    contents.putString("potion", "minecraft:water");
    wire::nbt::NbtCompound components;
    components.set("minecraft:potion_contents", wire::nbt::NbtTag::ofCompound(std::move(contents)));
    return ItemStack{
        .item = wire::common::Identifier::vanilla("potion"), .count = 1, .components = std::move(components)};
}

[[nodiscard]] const wire::common::Identifier& potion() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("potion");
    return instance;
}

/// parity: `dispensed.getOrDefault(POTION_CONTENTS, EMPTY).is(Potions.WATER)`, the only potion the
/// entry acts on.
[[nodiscard]] bool isWaterBottle(const ItemStack& stack) {
    const wire::nbt::NbtCompound* contents = stack.components.getCompound("minecraft:potion_contents");
    return contents != nullptr && contents->getString("potion", std::string{}) == "minecraft:water";
}

[[nodiscard]] const wire::common::Identifier& honeyBottle() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("honey_bottle");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& glowstone() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("glowstone");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& honeycomb() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("honeycomb");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& armorStand() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("armor_stand");
    return instance;
}

/// What a bucket with something alive in it lets out, and whether it pours water first.
///
/// parity: the ten `MobBucketItem` registrations. All of them carry water except the sulfur cube's,
/// whose fluid is `Fluids.EMPTY`: `emptyContents` answers true for that one without pouring
/// anything, so the bucket still comes back empty and the animal still gets out.
[[nodiscard]] std::optional<std::pair<std::string_view, bool>> caughtIn(std::string_view path) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kBuckets{
        {{"cod_bucket", "cod"},
         {"salmon_bucket", "salmon"},
         {"pufferfish_bucket", "pufferfish"},
         {"tropical_fish_bucket", "tropical_fish"},
         {"axolotl_bucket", "axolotl"},
         {"tadpole_bucket", "tadpole"},
         {"sulfur_cube_bucket", "sulfur_cube"}}};
    const auto found = std::ranges::find_if(kBuckets, [path](const auto& one) { return one.first == path; });
    if (found == kBuckets.end()) {
        return std::nullopt;
    }
    return std::pair{found->second, found->first != "sulfur_cube_bucket"};
}

/// parity: the sixteen `BoatDispenseItemBehavior` entries, whose item and entity share a name.
[[nodiscard]] bool isBoat(std::string_view path) {
    return path.ends_with("_boat") || path.ends_with("_raft") || path.ends_with("_chest_boat") ||
           path.ends_with("_chest_raft");
}

/// parity: `Direction.toYRot`, `(data2d & 3) * 90`: south is zero and, by the way that arithmetic
/// falls out, so up and down are both two hundred and seventy.
[[nodiscard]] float yawOf(BlockFace face) {
    switch (face) {
    case BlockFace::North:
        return 180.0F;
    case BlockFace::South:
        return 0.0F;
    case BlockFace::West:
        return 90.0F;
    case BlockFace::East:
    case BlockFace::Down:
    case BlockFace::Up:
        break;
    }
    return 270.0F;
}

[[nodiscard]] const wire::common::Identifier& powderSnowBucket() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("powder_snow_bucket");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& flintAndSteel() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("flint_and_steel");
    return instance;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] const wire::common::Identifier& witherSkull() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("wither_skeleton_skull");
    return instance;
}

/// parity: `RotationSegment.convertToSegment(Direction)`: north 0, east 4, south 8, west 12.
[[nodiscard]] std::int32_t rotationOf(registry::BlockFace face) {
    switch (face) {
    case registry::BlockFace::East:
        return 4;
    case registry::BlockFace::South:
        return 8;
    case registry::BlockFace::West:
        return 12;
    default:
        return 0;
    }
}

[[nodiscard]] bool isDispenser(std::string_view path) { return path == "dispenser" || path == "dropper"; }

[[nodiscard]] std::optional<BlockFace> faceOf(std::string_view facing) {
    if (facing == "down") {
        return BlockFace::Down;
    }
    if (facing == "up") {
        return BlockFace::Up;
    }
    if (facing == "north") {
        return BlockFace::North;
    }
    if (facing == "south") {
        return BlockFace::South;
    }
    if (facing == "west") {
        return BlockFace::West;
    }
    return facing == "east" ? std::optional{BlockFace::East} : std::nullopt;
}

[[nodiscard]] std::string_view nameOf(BlockFace face) {
    switch (face) {
    case BlockFace::Down:
        return "down";
    case BlockFace::Up:
        return "up";
    case BlockFace::North:
        return "north";
    case BlockFace::South:
        return "south";
    case BlockFace::West:
        return "west";
    case BlockFace::East:
        break;
    }
    return "east";
}

/// The two enums are both in vanilla's `Direction` order, which is what the Kotlin matched by name.
[[nodiscard]] wire::protocol::play::Direction directionOf(BlockFace face) {
    return static_cast<wire::protocol::play::Direction>(static_cast<std::uint8_t>(face));
}

[[nodiscard]] std::optional<std::int32_t> containerSizeOf(const Hooks& hooks, std::string_view path) {
    return hooks.containerSize ? hooks.containerSize(path) : containers::sizeOf(path);
}

[[nodiscard]] std::optional<std::int32_t> protocolTypeOf(const Hooks& hooks, const wire::common::Identifier& type) {
    if (hooks.protocolTypeOf) {
        return hooks.protocolTypeOf(type);
    }
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(type);
    return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
}

[[nodiscard]] std::int32_t nextEntityId(const Hooks& hooks) {
    return hooks.allocateEntityId ? hooks.allocateEntityId() : 0;
}

/// parity: `hurtAndBreak(1, level, null, ...)`: a tool a dispenser uses wears exactly like one in a
/// hand, and the slot is left empty when the last point of durability goes. The enchantments a hand
/// would consult are not read here, which only matters for an enchanted tool inside a dispenser.
[[nodiscard]] std::optional<ItemStack> wornOnce(const ItemStack& stack, random::RandomSource& random) {
    const storage::StoredItemStack held{
        .id = stack.item.toString(), .count = stack.count, .components = stack.components};
    const std::optional<storage::StoredItemStack> after = item::itemWear::hurt(held, 1, random);
    if (!after.has_value()) {
        return std::nullopt;
    }
    ItemStack left = stack;
    left.count = after->count;
    left.components = after->components;
    return left;
}

/// parity: `BuiltInLootTables.HARVEST_BEEHIVE`, which is three honeycombs and nothing else.
constexpr std::int32_t kHoneycombs = 3;

/// parity: `BeehiveBlock.HONEY_LEVEL`'s full value, which is when shears take anything at all.
constexpr std::int32_t kFullHive = 5;

/// parity: `EntityTypes.ITEM.getHeight() / 2` and `ItemEntity`'s own 0.2 upwards.
constexpr double kItemHalfHeight = 0.125;
constexpr double kPoppedUpwards = 0.2;

/// The stack a slot is left with once one item leaves it.
[[nodiscard]] std::optional<ItemStack> minusOne(const ItemStack& stack) {
    if (stack.count <= 1) {
        return std::nullopt;
    }
    ItemStack left = stack;
    left.count = stack.count - 1;
    return left;
}

/// parity: `DispenserBlockEntity.getRandomSlot`, which picks evenly among the slots with something
/// in by keeping the last of `seen` that a one-in-`seen` roll chose.
[[nodiscard]] std::optional<std::size_t> randomFullSlot(const blockInventory::Slots& slots,
                                                        random::RandomSource& random) {
    std::optional<std::size_t> chosen;
    std::int32_t seen = 0;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        if (!slots[slot].has_value()) {
            continue;
        }
        ++seen;
        if (random.nextInt(seen) == 0) {
            chosen = slot;
        }
    }
    return chosen;
}

/// parity: `CampfireBlock.canLight`, `CandleBlock.canLight` and `CandleCakeBlock.canLight`, which
/// are the same question three times: my own kind of block, out of the water, and not lit already.
[[nodiscard]] bool lightable(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    const bool kind =
        path == "campfire" || path == "soul_campfire" || path.ends_with("candle") || path.ends_with("candle_cake");
    return kind && propertyOf(stateId, "lit") == "false" && propertyOf(stateId, "waterlogged") != "true";
}

[[nodiscard]] bool isTntBlock(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "tnt";
}

/// How full a hive is, or nothing of the sort for a block that is not one.
[[nodiscard]] std::int32_t honeyOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || (block->path() != "beehive" && block->path() != "bee_nest")) {
        return -1;
    }
    const std::string level = propertyOf(stateId, "honey_level");
    return level.empty() ? -1 : std::stoi(level);
}

/// parity: `Block.popResource` with `BuiltInLootTables.HARVEST_BEEHIVE`: three honeycombs scattered
/// inside the hive's own square, thrown up at 0.2 like anything else a block drops.
void popHoneycombs(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                   const Hooks& hooks) {
    if (!context.gameRules.get(gameRules::kBlockDrops)) {
        return;
    }
    const auto scatter = [&context] { return (context.random.nextDouble() * 0.5) - 0.25; };
    static_cast<void>(context.world.spawnItem(DroppedItem{.entityId = nextEntityId(hooks),
                                                          .uuid = wire::common::Uuid::random(),
                                                          .item = wire::common::Identifier::vanilla("honeycomb"),
                                                          .count = kHoneycombs,
                                                          .x = x + 0.5 + scatter(),
                                                          .y = y + 0.5 + scatter() - kItemHalfHeight,
                                                          .z = z + 0.5 + scatter(),
                                                          .velocityX = (context.random.nextDouble() * 0.2) - 0.1,
                                                          .velocityY = kPoppedUpwards,
                                                          .velocityZ = (context.random.nextDouble() * 0.2) - 0.1}));
}

/// What a block-changing behaviour left in the slot. `handled` false means the item has no
/// behaviour of this kind and falls through to being thrown.
struct Special {
    bool handled = false;
    std::optional<ItemStack> left;
    /// What the behaviour changed in the world, when it changed a block rather than only a fluid.
    std::optional<BlockChange> change;
    /// parity: `OptionalDispenseItemBehavior.setSuccess(false)`, which is not a fall through: the
    /// item stays in the slot, nothing is thrown, and the dispenser clicks.
    bool refused = false;
};

/// parity: `Level.gameEvent(null, event, pos)` from a dispenser's behaviour, sent from the middle of
/// the square it acted on. The tracker is asked first because most turns have nothing listening.
void heard(WorldState& world, const std::string_view event, const std::int32_t x, const std::int32_t y,
           const std::int32_t z, const std::int64_t gameTime) {
    if (!world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        return;
    }
    vibrations::emit(world, event, static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                     static_cast<double>(z) + 0.5, gameTime);
}

/// parity: `ShulkerBoxDispenseBehavior` over `BlockItem.place`, whose `DirectionalPlaceContext`
/// answers the clicked face and nothing else that a shulker box reads.
[[nodiscard]] Special placeShulkerBox(const fluidFlow::Context& context, std::int32_t tx, std::int32_t ty,
                                      std::int32_t tz, BlockFace out, const ItemStack& stack, const Hooks& hooks) {
    const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
    const registry::BlockState* box = registry::blocks::named(stack.item.path());
    if (!target.has_value() || box == nullptr || !registry::blockShapes::isReplaceable(*target)) {
        return Special{.refused = true};
    }
    const std::optional<std::int32_t> below = context.world.blockState(tx, ty - 1, tz);
    const BlockFace face = below.has_value() && registry::blocks::isAir(*below) ? out : BlockFace::Up;
    const std::optional<std::int32_t> placed =
        registry::blockStateDefinitions::stateIdOf(stack.item, {{"facing", std::string{nameOf(face)}}}, box->stateId);
    if (!placed.has_value() || !context.world.setBlockState(tx, ty, tz, *placed)) {
        return Special{.refused = true};
    }
    // parity: `Level.setBlock` gives the box its block entity, and `BlockItem.updateCustomBlockEntityTag`
    // puts back whatever the item was carrying.
    server::world::Chunk* chunk = context.world.chunkIfLoaded(tx >> 4, tz >> 4);
    if (chunk != nullptr) {
        wire::nbt::NbtCompound data = blockEntityDefaults::of(stack.item, *placed);
        if (const wire::nbt::NbtCompound* carried = stack.components.getCompound(shulkers::kContainer);
            carried != nullptr) {
            shulkers::restore(data, *carried);
        }
        chunk->setBlockEntity(server::world::ChunkBlockEntity{tx & 15, ty, tz & 15, stack.item, std::move(data)});
        chunk->markDirty();
    }
    heard(context.world, "block_place", tx, ty, tz, hooks.gameTime);
    return Special{
        .handled = true, .left = minusOne(stack), .change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = *placed}};
}

/// The block-changing behaviours: a full bucket pours, an empty one fills, and flint and steel
/// lights a fire.
///
/// parity: `DispenseItemBehavior.bootStrap`'s filled-bucket, empty-bucket and flint-and-steel
/// entries. The rest of that table makes entities.
[[nodiscard]] Special dispenseOnBlock(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                      BlockFace out, const ItemStack& stack, const Hooks& hooks) {
    const std::int32_t tx = x + registry::stepX(out);
    const std::int32_t ty = y + registry::stepY(out);
    const std::int32_t tz = z + registry::stepZ(out);
    // parity: the carved pumpkin entry, an `OptionalDispenseItemBehavior`: it puts the pumpkin down
    // only when the square in front is empty **and** `canSpawnGolem` says a figure would stand up.
    // Anything else is a refusal, so the pumpkin stays in the dispenser instead of being thrown, and
    // that is what lets a dispenser finish an iron golem.
    if (stack.item == carvedPumpkin()) {
        const std::optional<std::int32_t> front = context.world.blockState(tx, ty, tz);
        if (!front.has_value() || !registry::blocks::isAir(*front) ||
            !golemPatterns::canComplete(context.world, tx, ty, tz, hooks.isCopper)) {
            return Special{.refused = true};
        }
        const registry::BlockState* pumpkin = registry::blocks::named("carved_pumpkin");
        if (pumpkin == nullptr || !context.world.setBlockState(tx, ty, tz, pumpkin->stateId)) {
            return Special{.refused = true};
        }
        heard(context.world, "block_place", tx, ty, tz, hooks.gameTime);
        return Special{.handled = true,
                       .left = minusOne(stack),
                       .change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = pumpkin->stateId}};
    }
    // parity: the wither skeleton skull entry, another `OptionalDispenseItemBehavior`: the skull goes
    // down only on an empty square that completes the soul sand T, and the boss is checked for right
    // after. A refusal keeps the skull rather than throwing it. The jar tries `dispenseEquipment`
    // first, putting the skull on the head of a mob standing in front; nothing here wears one, so a
    // square that does not complete the T is a plain refusal.
    if (stack.item == witherSkull()) {
        const std::optional<std::int32_t> front = context.world.blockState(tx, ty, tz);
        if (!front.has_value() || !registry::blocks::isAir(*front) ||
            !witherPatterns::canComplete(context.world, tx, ty, tz, hooks.isWitherBase)) {
            return Special{.refused = true};
        }
        const std::optional<std::int32_t> placed = registry::blockStateDefinitions::stateIdOf(
            witherSkull(), {{"rotation", std::to_string(rotationOf(out))}}, std::nullopt);
        if (!placed.has_value() || !context.world.setBlockState(tx, ty, tz, *placed)) {
            return Special{.refused = true};
        }
        // parity: `Level.setBlock` gives the skull its own block entity, which is what the jar then
        // looks for before it checks the pattern.
        if (server::world::Chunk* chunk = context.world.chunkIfLoaded(tx >> 4, tz >> 4); chunk != nullptr) {
            chunk->setBlockEntity(server::world::ChunkBlockEntity{tx & 15, ty, tz & 15, stack.item,
                                                                  blockEntityDefaults::of(stack.item, *placed)});
            chunk->markDirty();
        }
        heard(context.world, "block_place", tx, ty, tz, hooks.gameTime);
        if (hooks.skullPlaced) {
            hooks.skullPlaced(tx, ty, tz);
        }
        return Special{.handled = true,
                       .left = minusOne(stack),
                       .change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = *placed}};
    }
    // parity: the brush entry, which brushes an armadillo standing in the square in front and
    // refuses when there is none. Brushing is not implemented anywhere in this build, not even by
    // hand, so this refuses always: wrong only when an armadillo is standing there, and right the
    // rest of the time. What it does get right either way is that a refusal **keeps** the brush,
    // because an `OptionalDispenseItemBehavior` never throws what it failed on.
    if (stack.item == brush()) {
        return Special{.refused = true};
    }
    // parity: the armor stand entry, which stands one in the square in front whatever is there, at
    // the bottom middle of it and turned the way the dispenser looks.
    if (stack.item == armorStand()) {
        const std::optional<std::int32_t> protocolType = protocolTypeOf(hooks, stack.item);
        if (!protocolType.has_value()) {
            return {};
        }
        auto stand = std::make_unique<entity::ServerEntity>(nextEntityId(hooks), wire::common::Uuid::random(),
                                                            stack.item, *protocolType, tx + 0.5, ty, tz + 0.5);
        stand->yaw = yawOf(out);
        static_cast<void>(context.world.spawnEntity(std::move(stand)));
        return Special{.handled = true, .left = minusOne(stack)};
    }
    // parity: `BoatDispenseItemBehavior`, which wants water in front or water under an empty square
    // and throws the boat as an item when it finds neither. The boat lands just outside the
    // dispenser, half its own width past the face.
    if (isBoat(stack.item.path())) {
        const std::optional<std::int32_t> front = context.world.blockState(tx, ty, tz);
        const std::optional<std::int32_t> under = context.world.blockState(tx, ty - 1, tz);
        double lift = 0.0;
        if (front.has_value() && fluidFlow::kindOf(*front) == FluidKind::Water) {
            lift = 1.0;
        } else if (!front.has_value() || !registry::blocks::isAir(*front) || !under.has_value() ||
                   fluidFlow::kindOf(*under) != FluidKind::Water) {
            return {};
        }
        const std::optional<std::int32_t> protocolType = protocolTypeOf(hooks, stack.item);
        if (!protocolType.has_value()) {
            return {};
        }
        const double reach = kBoatClearance + (registry::entityTypeShapes::widthOf(stack.item) / 2.0);
        auto boat = std::make_unique<entity::ServerEntity>(
            nextEntityId(hooks), wire::common::Uuid::random(), stack.item, *protocolType,
            x + 0.5 + (registry::stepX(out) * reach), y + 0.5 + (registry::stepY(out) * kBoatLift) + lift,
            z + 0.5 + (registry::stepZ(out) * reach));
        boat->yaw = yawOf(out);
        static_cast<void>(context.world.spawnEntity(std::move(boat)));
        return Special{.handled = true, .left = minusOne(stack)};
    }
    // parity: `SpawnEggItemBehavior`, which `DispenserBlock.getDefaultDispenseMethod` picks for any
    // spawn egg: the animal comes out in the square in front. Vanilla reads the type off the item's
    // `entity_data` component; this build has no default components, and the name carries the same
    // fact. Measured in `dispensahuevo`.
    if (stack.item.path().ends_with("_spawn_egg")) {
        const std::string_view path = stack.item.path();
        const wire::common::Identifier animal =
            wire::common::Identifier::vanilla(std::string{path.substr(0, path.size() - kSpawnEggSuffix)});
        const std::optional<std::int32_t> protocolType = protocolTypeOf(hooks, animal);
        if (!protocolType.has_value()) {
            return {};
        }
        auto spawned =
            std::make_unique<entity::ServerEntity>(nextEntityId(hooks), wire::common::Uuid::random(), animal,
                                                   *protocolType, tx + 0.5, static_cast<double>(ty), tz + 0.5);
        static_cast<void>(context.world.spawnEntity(std::move(spawned)));
        // parity: the `ENTITY_PLACE` of this one comes from the **dispenser's** square, not from the
        // square the animal landed in.
        heard(context.world, "entity_place", x, y, z, hooks.gameTime);
        return Special{.handled = true, .left = minusOne(stack)};
    }
    // parity: `MobBucketItem`: the water goes down like any full bucket's, the animal comes out at
    // the bottom middle of the square, and the bucket comes back empty. Measured in `dispensapez`.
    if (const std::optional<std::pair<std::string_view, bool>> caught = caughtIn(stack.item.path());
        caught.has_value()) {
        if (caught->second) {
            const std::optional<buckets::Result> poured =
                buckets::useOn(context, buckets::waterBucket(), tx, ty, tz, directionOf(out), false);
            if (!poured.has_value()) {
                return {};
            }
            heard(context.world, "fluid_place", tx, ty, tz, hooks.gameTime);
        }
        const wire::common::Identifier animal = wire::common::Identifier::vanilla(std::string{caught->first});
        const std::optional<std::int32_t> protocolType = protocolTypeOf(hooks, animal);
        if (protocolType.has_value()) {
            // parity: `EntityType.create(..., shouldOffsetY = true)` drops it a block onto whatever is
            // in the square, and an empty square or one full of water stops nothing: it lands on the
            // floor of it.
            auto spawned =
                std::make_unique<entity::ServerEntity>(nextEntityId(hooks), wire::common::Uuid::random(), animal,
                                                       *protocolType, tx + 0.5, static_cast<double>(ty), tz + 0.5);
            static_cast<void>(context.world.spawnEntity(std::move(spawned)));
            heard(context.world, "entity_place", tx, ty, tz, hooks.gameTime);
        }
        const std::optional<ItemStack> left = minusOne(stack);
        return Special{.handled = true,
                       .left = left.has_value() ? left
                                                : std::optional{ItemStack{.item = buckets::emptyBucket(), .count = 1}}};
    }
    // parity: `SolidBucketItem.emptyContents` under the dispenser's filled-bucket entry: the powder
    // snow goes down on an empty square and the bucket comes back empty. Anywhere else the behaviour
    // answers false and the bucket is thrown, which is what falling through does.
    if (stack.item == powderSnowBucket()) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        const registry::BlockState* snow = registry::blocks::named("powder_snow");
        if (!target.has_value() || !registry::blocks::isAir(*target) || snow == nullptr ||
            !context.world.setBlockState(tx, ty, tz, snow->stateId)) {
            return {};
        }
        heard(context.world, "fluid_place", tx, ty, tz, hooks.gameTime);
        const BlockChange change{.x = tx, .y = ty, .z = tz, .stateId = snow->stateId};
        const std::optional<ItemStack> left = minusOne(stack);
        return Special{.handled = true,
                       .left = left.has_value() ? left
                                                : std::optional{ItemStack{.item = buckets::emptyBucket(), .count = 1}},
                       .change = change};
    }
    // parity: `ShulkerBoxDispenseBehavior`, which places the box rather than throwing it. The face it
    // lands on is the dispenser's own only when the square under the target is empty; over anything
    // solid it is `UP`. A box that cannot go down is the click of a refusal, not a throw.
    if (shulkers::isShulkerBox(stack.item.path())) {
        return placeShulkerBox(context, tx, ty, tz, out, stack, hooks);
    }
    if (buckets::isBucket(stack.item)) {
        const std::optional<buckets::Result> result =
            buckets::useOn(context, stack.item, tx, ty, tz, directionOf(out), false);
        if (!result.has_value()) {
            return {};
        }
        // parity: the filled bucket raises `FLUID_PLACE` from `playEmptySound` and the empty one
        // raises `FLUID_PICKUP`, both at the square in front rather than at the dispenser.
        heard(context.world, stack.item.path() == "bucket" ? "fluid_pickup" : "fluid_place", tx, ty, tz,
              hooks.gameTime);
        // parity: `consumeWithRemainder`, which puts what the bucket became back in the slot.
        const std::optional<ItemStack> left = minusOne(stack);
        if (left.has_value()) {
            return Special{.handled = true, .left = left};
        }
        if (!result->replacement.has_value()) {
            return {};
        }
        return Special{.handled = true, .left = ItemStack{.item = *result->replacement, .count = 1}};
    }
    // parity: `DispenseItemBehavior`'s tnt entry, which is not a throw: the dispenser builds the
    // primed entity itself at the square in front, and raises `ENTITY_PLACE` rather than the
    // `PRIME_FUSE` a block being lit raises. With the `tnt_explodes` rule off it answers a failure,
    // which is the click a dispenser makes when a behaviour says no.
    if (stack.item == tnt()) {
        if (!context.gameRules.get(gameRules::kTntExplodes)) {
            return Special{.refused = true};
        }
        context.world.primeTnt(tx, ty, tz, "entity_place");
        return Special{.handled = true, .left = minusOne(stack)};
    }
    // parity: `DispenseItemBehavior`'s bone meal entry, which runs `BoneMealItem.growCrop` on the
    // square in front and sparkles there. A valid target spends the item even when the growth roll
    // does nothing, and anything else is a refusal rather than a throw.
    if (stack.item == boneMealItem()) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        if (!target.has_value() || !boneMeal::grows(*target)) {
            return Special{.refused = true};
        }
        std::optional<BlockChange> grown = boneMeal::apply(context.world, tx, ty, tz, context.random);
        if (grown.has_value()) {
            grown->effects.emplace_back(
                blockEffect::Effect{.type = blockEffect::Effect::kPlantGrowth, .data = kGrowthSparkles});
        }
        return Special{.handled = true, .left = minusOne(stack), .change = grown};
    }
    // parity: the glowstone entry, which only knows the respawn anchor: a full one refuses, and
    // anything that is not an anchor falls through to being thrown.
    if (stack.item == glowstone()) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        if (!target.has_value() || !respawnAnchors::isAnchor(*target)) {
            return {};
        }
        const std::optional<BlockChange> charged = respawnAnchors::charge(context.world, tx, ty, tz);
        if (!charged.has_value()) {
            return Special{.refused = true};
        }
        // parity: `RespawnAnchorBlock.charge` raises `BLOCK_CHANGE` beside the sound.
        heard(context.world, "block_change", tx, ty, tz, hooks.gameTime);
        return Special{.handled = true, .left = minusOne(stack), .change = charged};
    }
    // parity: the honeycomb entry, which waxes the block in front. Not being waxable is a plain
    // fall through to `super.execute`, so the comb is thrown rather than refused.
    if (stack.item == honeycomb()) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        const std::optional<std::int32_t> waxed =
            target.has_value() ? waxing::waxed(*target) : std::optional<std::int32_t>{};
        if (!waxed.has_value() || !context.world.setBlockState(tx, ty, tz, *waxed)) {
            return {};
        }
        BlockChange change{.x = tx, .y = ty, .z = tz, .stateId = *waxed};
        change.effects.emplace_back(blockEffect::Effect{.type = blockEffect::Effect::kWaxOn, .data = 0});
        return Special{.handled = true, .left = minusOne(stack), .change = change};
    }
    // parity: `FlintAndSteelDispenseItemBehavior`, which answers success for everything except a
    // tnt block it could not prime: aimed at a wall it wears one point and nothing else, and it is
    // never thrown.
    if (stack.item == flintAndSteel()) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        std::optional<BlockChange> change;
        if (fires::ignite(context.world, tx, ty, tz, context.random)) {
            const std::optional<std::int32_t> fire = context.world.blockState(tx, ty, tz);
            change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = fire.value_or(0)};
            heard(context.world, "block_place", tx, ty, tz, hooks.gameTime);
        } else if (target.has_value() && lightable(*target)) {
            const std::optional<std::int32_t> lit = registry::blockStateDefinitions::stateIdOf(
                *registry::blockStateDefinitions::blockOf(*target), {{"lit", "true"}}, *target);
            if (lit.has_value() && context.world.setBlockState(tx, ty, tz, *lit)) {
                change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = *lit};
                heard(context.world, "block_change", tx, ty, tz, hooks.gameTime);
            }
        } else if (target.has_value() && isTntBlock(*target)) {
            // parity: `TntBlock.prime` then `removeBlock`, and with `tnt_explodes` off the block
            // stays where it is and the behaviour answers a failure.
            if (!context.gameRules.get(gameRules::kTntExplodes)) {
                return Special{.refused = true};
            }
            context.world.primeTnt(tx, ty, tz, "prime_fuse");
            static_cast<void>(context.world.setBlockState(tx, ty, tz, registry::blocks::air().stateId));
            change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = registry::blocks::air().stateId};
        }
        return Special{.handled = true, .left = wornOnce(stack, context.random), .change = change};
    }
    // parity: the potion entry, which only knows one potion: a **water** bottle turns the block in
    // front into mud when it is in `convertible_to_mud`, and leaves the empty glass. Anything else,
    // any other potion included, falls through to being thrown. Its `FLUID_PLACE` also comes from the
    // dispenser's own square.
    if (stack.item == potion() && isWaterBottle(stack)) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        const wire::common::Identifier* block =
            target.has_value() ? registry::blockStateDefinitions::blockOf(*target) : nullptr;
        if (block == nullptr || !hooks.convertsToMud || !hooks.convertsToMud(*block)) {
            return {};
        }
        const registry::BlockState* mud = registry::blocks::named("mud");
        if (mud == nullptr || !context.world.setBlockState(tx, ty, tz, mud->stateId)) {
            return {};
        }
        heard(context.world, "fluid_place", x, y, z, hooks.gameTime);
        const std::optional<ItemStack> left = minusOne(stack);
        return Special{.handled = true,
                       .left = left.has_value() ? left : ItemStack{.item = glassBottle(), .count = 1},
                       .change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = mud->stateId}};
    }
    // parity: the glass bottle entry: honey out of a hive that is full, a water bottle out of water,
    // and anything else falls through to being thrown. Its `FLUID_PICKUP` comes from the
    // **dispenser's** own square, not from the one in front, which is the opposite of the bucket's.
    if (stack.item == glassBottle()) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        std::optional<ItemStack> filled;
        std::optional<BlockChange> change;
        if (target.has_value() && honeyOf(*target) >= kFullHive) {
            const std::optional<std::int32_t> emptied = registry::blockStateDefinitions::stateIdOf(
                *registry::blockStateDefinitions::blockOf(*target), {{"honey_level", "0"}}, *target);
            if (emptied.has_value() && context.world.setBlockState(tx, ty, tz, *emptied)) {
                filled = ItemStack{.item = honeyBottle(), .count = 1};
                change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = *emptied};
                if (hooks.releaseHive) {
                    hooks.releaseHive(tx, ty, tz, *target);
                }
            }
        } else if (target.has_value() && registry::blockSupport::holdsWater(*target)) {
            filled = waterBottle();
        }
        if (!filled.has_value()) {
            return {};
        }
        heard(context.world, "fluid_pickup", x, y, z, hooks.gameTime);
        // parity: `consumeWithRemainder`. What is left of the stack stays in the slot and the filled
        // bottle only takes its place when it was the last one, which is the same shape the bucket
        // above uses.
        const std::optional<ItemStack> left = minusOne(stack);
        return Special{.handled = true, .left = left.has_value() ? left : filled, .change = change};
    }
    // parity: `ShearsDispenseItemBehavior.tryShearBeehive`, which only touches a hive that is full.
    // Shearing a sheep in front of it is the other half and needs the mob.
    if (stack.item == shears()) {
        const std::optional<std::int32_t> target = context.world.blockState(tx, ty, tz);
        if (!target.has_value() || honeyOf(*target) < kFullHive) {
            return Special{.refused = true};
        }
        const std::optional<std::int32_t> emptied = registry::blockStateDefinitions::stateIdOf(
            *registry::blockStateDefinitions::blockOf(*target), {{"honey_level", "0"}}, *target);
        if (!emptied.has_value() || !context.world.setBlockState(tx, ty, tz, *emptied)) {
            return Special{.refused = true};
        }
        popHoneycombs(context, tx, ty, tz, hooks);
        heard(context.world, "shear", tx, ty, tz, hooks.gameTime);
        if (hooks.releaseHive) {
            hooks.releaseHive(tx, ty, tz, *target);
        }
        return Special{.handled = true,
                       .left = wornOnce(stack, context.random),
                       .change = BlockChange{.x = tx, .y = ty, .z = tz, .stateId = *emptied}};
    }
    return {};
}

/// parity: `MinecartDispenseItemBehavior`'s `1.125`, an eighth past the square in front.
constexpr double kMinecartReach = 1.125;

/// parity: `MinecartDispenseItemBehavior`. A dispenser puts a cart **on the rail** in front of it,
/// or on the one a step below when the square in front is air, and throws the item like anything
/// else when there is no rail either way. The heights are vanilla's own four numbers.
[[nodiscard]] bool dispenseMinecart(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace out,
                                    const ItemStack& stack, const Hooks& hooks) {
    const std::int32_t fx = x + registry::stepX(out);
    const std::int32_t fy = y + registry::stepY(out);
    const std::int32_t fz = z + registry::stepZ(out);
    const std::optional<std::int32_t> front = world.blockState(fx, fy, fz);
    if (!front.has_value()) {
        return false;
    }
    const auto slopes = [](std::int32_t stateId) {
        return registry::blockStateDefinitions::propertiesOf(stateId)["shape"].starts_with("ascending");
    };
    double lift = 0.0;
    if (rails::isRail(*front)) {
        lift = slopes(*front) ? 0.6 : 0.1;
    } else {
        const std::optional<std::int32_t> below = world.blockState(fx, fy - 1, fz);
        if (*front != registry::blocks::air().stateId || !below.has_value() || !rails::isRail(*below)) {
            return false;
        }
        lift = out != BlockFace::Down && slopes(*below) ? -0.4 : -0.9;
    }
    const std::optional<std::int32_t> protocolType = protocolTypeOf(hooks, stack.item);
    if (!protocolType.has_value()) {
        return false;
    }
    // parity: the cart lands an eighth of a block past the square in front, which is what puts it on
    // the rail rather than inside the dispenser.
    auto cart = std::make_unique<entity::ServerEntity>(nextEntityId(hooks), wire::common::Uuid::random(), stack.item,
                                                       *protocolType, x + 0.5 + (registry::stepX(out) * kMinecartReach),
                                                       y + registry::stepY(out) + lift,
                                                       z + 0.5 + (registry::stepZ(out) * kMinecartReach));
    static_cast<void>(world.spawnEntity(std::move(cart)));
    return true;
}

/// The container in front: where it is and how many slots it has.
struct Target {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t size = 0;
    std::string path;
};

/// The container in front, or nothing when the block in front is not one.
[[nodiscard]] std::optional<Target> containerInFront(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                     BlockFace out, const Hooks& hooks) {
    const Target target{.x = x + registry::stepX(out), .y = y + registry::stepY(out), .z = z + registry::stepZ(out)};
    const std::optional<std::int32_t> stateId = world.blockState(target.x, target.y, target.z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> size = containerSizeOf(hooks, block->path());
    if (!size.has_value()) {
        return std::nullopt;
    }
    return Target{.x = target.x, .y = target.y, .z = target.z, .size = *size, .path = std::string{block->path()}};
}

/// Puts one item of the stack into the container, answering whether a slot took it.
[[nodiscard]] bool insertOne(WorldState& world, const Target& into, const ItemStack& stack, BlockFace from,
                             const Hooks& hooks) {
    world.unpackLoot(into.x, into.y, into.z);
    server::world::Chunk* chunk = world.chunkIfLoaded(into.x >> 4, into.z >> 4);
    server::world::ChunkBlockEntity* blockEntity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(into.x & 15, into.y, into.z & 15);
    if (blockEntity == nullptr) {
        return false;
    }
    blockInventory::Slots slots = blockInventory::readFor(into.path, blockEntity->data(), into.size);
    // parity: `DropperBlock.dispenseFrom` hands the stack to `HopperBlockEntity.addItem`, so a
    // dropper obeys the same doors a hopper does: the face, the slot's own rules, and the crafter's
    // way of spreading what it is fed.
    for (const std::int32_t slot : containers::slotsForFace(into.path, into.size, from)) {
        if (slot < 0 || static_cast<std::size_t>(slot) >= slots.size()) {
            continue;
        }
        std::optional<ItemStack>& there = slots[static_cast<std::size_t>(slot)];
        if (there.has_value() && (there->item != stack.item || there->components != stack.components ||
                                  there->count >= containers::maxStackFor(into.path, stack.item))) {
            continue;
        }
        if (!containers::acceptsThroughFace(into.path, slot, stack.item, !there.has_value())) {
            continue;
        }
        if (into.path == crafters::block().path() && !crafters::accepts(blockEntity->data(), slots, slot, stack)) {
            continue;
        }
        const bool wasEmpty = !there.has_value();
        if (there.has_value()) {
            there->count += 1;
        } else {
            there = ItemStack{.item = stack.item, .count = 1, .components = stack.components};
        }
        blockInventory::writeFor(into.path, blockEntity->data(), slots);
        // parity: `tryMoveInItem` reaches `setItem` only when the slot it lands in was empty.
        if (wasEmpty && slot == furnaces::kInputSlot) {
            furnaces::inputChanged(into.path, blockEntity->data(), hooks.recipeFor);
        }
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, into.x, into.y, into.z, comparatorReading::of(world));
        return true;
    }
    return false;
}

/// parity: `DefaultDispenseItemBehavior.spawnItem`: out of the face, a little below the middle.
void throwOut(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace out, const ItemStack& stack,
              random::RandomSource& random, const Hooks& hooks) {
    const double centreX = x + 0.5 + (kDispenseReach * registry::stepX(out));
    const double centreZ = z + 0.5 + (kDispenseReach * registry::stepZ(out));
    double centreY = y + 0.5 + (kDispenseReach * registry::stepY(out));
    centreY -= out == BlockFace::Up || out == BlockFace::Down ? kVerticalDrop : kSidewaysDrop;
    const double push = (random.nextDouble() * kSpeedSpread) + kSpeedLow;
    const double spread = kSpreadPerAccuracy * kDefaultAccuracy;
    // The three rolls are in this order because vanilla's are, and a braced list is sequenced.
    world.spawnItem(DroppedItem{.entityId = nextEntityId(hooks),
                                .uuid = wire::common::Uuid::random(),
                                .item = stack.item,
                                .count = 1,
                                .x = centreX,
                                .y = centreY,
                                .z = centreZ,
                                .velocityX = random.triangle(registry::stepX(out) * push, spread),
                                .velocityY = random.triangle(kThrowLift, spread),
                                .velocityZ = random.triangle(registry::stepZ(out) * push, spread),
                                .components = stack.components});
}

/// parity: `ProjectileDispenseBehavior`, which leaves from where the item's own config says and is
/// shot along the face with its power and its spread.
void shoot(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace out,
           const entity::projectiles::DispenseShot& config, const ItemStack& stack, const Hooks& hooks) {
    const std::optional<std::int32_t> protocolType = protocolTypeOf(hooks, config.kind->type);
    if (!protocolType.has_value()) {
        return;
    }
    entity::ServerEntity& shot = entity::projectiles::spawn(
        context.world, *config.kind, nextEntityId(hooks), *protocolType,
        x + 0.5 + (config.reach * registry::stepX(out)), y + 0.5 + (config.reach * registry::stepY(out)) + config.lift,
        z + 0.5 + (config.reach * registry::stepZ(out)), registry::stepX(out), registry::stepY(out),
        registry::stepZ(out), config.power, config.uncertainty, context.random);
    // parity: `ProjectileItem.asProjectile` hands the stack to the projectile, which saves it as its
    // `Item`: the fire charge a fireball came from travels with it.
    if (config.carries) {
        shot.heldItem = stack.item.path();
    }
    // parity: `ArrowItem.asProjectile`, whose arrow keeps the whole stack and may be picked up.
    if (config.kind->sticks) {
        shot.heldItem = stack.item.path();
        shot.heldItemComponents = stack.components;
        shot.arrowPickup = entity::ServerEntity::ArrowPickup::Allowed;
        // parity: `ThrownTrident`'s `ID_FOIL`, the stack's glint.
        const wire::nbt::NbtTag* glint = stack.components.find("minecraft:enchantment_glint_override");
        const wire::nbt::NbtCompound* enchanted = stack.components.getCompound("minecraft:enchantments");
        shot.tridentFoil = glint != nullptr && glint->get<std::int8_t>() != nullptr
                               ? *glint->get<std::int8_t>() != 0
                               : enchanted != nullptr && !enchanted->empty();
    }
}

} // namespace

const wire::common::Identifier& tickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("dispenser");
    return instance;
}

std::optional<BlockChange> onNeighbourChange(WorldState& world, std::string_view worldName, std::int32_t x,
                                             std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isDispenser(block->path())) {
        return std::nullopt;
    }
    const bool triggered = propertyOf(*stateId, "triggered") == "true";
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0 ||
                        redstone::bestNeighbourSignal(world, worldName, x, y + 1, z) > 0;
    if (signal == triggered) {
        return std::nullopt;
    }
    if (signal) {
        // The turn goes under the block's own name: a dropper's is `minecraft:dropper`.
        world.scheduleBlockTick(x, y, z, *block, kDelay);
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"triggered", signal ? "true" : "false"}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

Result fire(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, const Hooks& hooks) {
    Result result;
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return result;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isDispenser(block->path())) {
        return result;
    }
    // parity: `DispenserBlockEntity.getRandomSlot` unpacks its own table first.
    world.unpackLoot(x, y, z);
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* blockEntity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (blockEntity == nullptr) {
        return result;
    }
    blockInventory::Slots slots = blockInventory::read(blockEntity->data(), kSize);
    const std::optional<std::size_t> slot = randomFullSlot(slots, context.random);
    if (!slot.has_value()) {
        // parity: `DispenserBlock.dispenseFrom`, which clicks at nothing when it is empty. That
        // click is the only noise the dispenser itself makes: a turn that dispenses something makes
        // whatever noise the behaviour makes, and none of its own.
        result.effects.emplace_back(blockEffect::Effect{.type = blockEffect::Effect::kDispenserFail, .data = 0});
        heard(world, "block_activate", x, y, z, hooks.gameTime);
        return result;
    }
    const std::optional<BlockFace> out = faceOf(propertyOf(*stateId, "facing"));
    if (!out.has_value()) {
        return result;
    }
    const ItemStack stack = *slots[*slot];

    // parity: `DropperBlock.getDispenseMethod`, which answers the default behaviour whatever the
    // item is: a dropper never shoots and never pours, it only throws or hands over.
    const bool dropper = block->path() == "dropper";

    // parity: `FireworkRocketItem`'s dispense config: half a block and a hair out of the face, a
    // power of a half and a spread of one, and the firework's own launch sound.
    if (!dropper && stack.item.path() == "firework_rocket" && hooks.launchFirework) {
        constexpr double kJustOutside = 0.5000099999997474;
        hooks.launchFirework(x + 0.5 + (kJustOutside * registry::stepX(*out)),
                             y + 0.5 + (kJustOutside * registry::stepY(*out)),
                             z + 0.5 + (kJustOutside * registry::stepZ(*out)), registry::stepX(*out),
                             registry::stepY(*out), registry::stepZ(*out), stack);
        result.effects.emplace_back(blockEffect::Effect{.type = blockEffect::Effect::kFireworkShoot, .data = 0});
        result.effects.emplace_back(
            blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = static_cast<std::int32_t>(*out)});
        slots[*slot] = minusOne(stack);
        blockInventory::write(blockEntity->data(), slots);
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
        result.fired = true;
        return result;
    }
    // parity: `DispenserBlock.registerProjectileBehavior`, which shoots rather than throws.
    const std::optional<entity::projectiles::DispenseShot> projectile =
        dropper ? std::nullopt : entity::projectiles::dispensed(stack.item);
    if (projectile.has_value()) {
        shoot(context, x, y, z, *out, *projectile, stack, hooks);
        slots[*slot] = minusOne(stack);
        blockInventory::write(blockEntity->data(), slots);
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
        result.fired = true;
        return result;
    }

    // parity: the `DispenseItemBehavior`s that change blocks. The ones that make an entity fall
    // through to being thrown.
    const Special special = dropper ? Special{} : dispenseOnBlock(context, x, y, z, *out, stack, hooks);
    if (special.refused) {
        result.effects.emplace_back(blockEffect::Effect{.type = blockEffect::Effect::kDispenserFail, .data = 0});
        // parity: `DispenserBlock.dispenseFrom` writes the slot back whatever the behaviour answered,
        // and `setItem` calls `setChanged`, so even a refused turn leaves the chunk dirty.
        chunk->markDirty();
        return result;
    }
    if (special.handled) {
        if (special.change.has_value()) {
            result.changes.push_back(*special.change);
        }
        slots[*slot] = special.left;
        blockInventory::write(blockEntity->data(), slots);
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
        result.fired = true;
        return result;
    }

    // parity: a minecart goes on the rail rather than into a container or the air.
    if (!dropper && stack.item.path().ends_with("minecart") && dispenseMinecart(world, x, y, z, *out, stack, hooks)) {
        slots[*slot] = minusOne(stack);
        blockInventory::write(blockEntity->data(), slots);
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
        result.fired = true;
        return result;
    }
    const std::optional<Target> into = containerInFront(world, x, y, z, *out, hooks);
    if (into.has_value()) {
        if (!insertOne(world, *into, stack, registry::opposite(*out), hooks)) {
            // parity: `DropperBlock.dispenseFrom` writes back the stack it could not hand over, so a
            // full container still marks the block entity changed.
            chunk->markDirty();
            return result;
        }
    } else {
        throwOut(world, x, y, z, *out, stack, context.random, hooks);
    }
    slots[*slot] = minusOne(stack);
    blockInventory::write(blockEntity->data(), slots);
    chunk->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    result.fired = true;
    return result;
}

} // namespace basalt::server::runtime::world::dispensers
