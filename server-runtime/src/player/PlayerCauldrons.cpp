#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerCauldrons.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>

#include <string>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;
using storage::StoredItemStack;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;

/// parity: `LayeredCauldronBlock.MAX_FILL_LEVEL`.
constexpr std::int32_t kFullLevel = 3;
constexpr std::string_view kPotionContents = "minecraft:potion_contents";

enum class Contents : std::uint8_t { Empty, Water, Lava, PowderSnow };

[[nodiscard]] std::optional<Contents> contentsOf(std::string_view path) {
    if (path == "cauldron") {
        return Contents::Empty;
    }
    if (path == "water_cauldron") {
        return Contents::Water;
    }
    if (path == "lava_cauldron") {
        return Contents::Lava;
    }
    if (path == "powder_snow_cauldron") {
        return Contents::PowderSnow;
    }
    return std::nullopt;
}

[[nodiscard]] bool isWaterPotion(const StoredItemStack& stack) {
    const wire::nbt::NbtCompound* contents = stack.components.getCompound(kPotionContents);
    return stack.id == "minecraft:potion" && contents != nullptr &&
           contents->getString("potion", std::string_view{}) == "minecraft:water";
}

[[nodiscard]] StoredItemStack waterPotion() {
    wire::nbt::NbtCompound contents;
    contents.putString("potion", "minecraft:water");
    StoredItemStack potion{.id = "minecraft:potion", .count = 1, .components = {}};
    potion.components.set(kPotionContents, wire::nbt::NbtTag::ofCompound(std::move(contents)));
    return potion;
}

[[nodiscard]] bool carries(const PlayerSession& player, const StoredItemStack& stack) {
    for (std::int32_t slot = 0; slot <= PlayerInventory::kOffhandSlot; ++slot) {
        const std::optional<StoredItemStack>& held = player.inventory.at(slot);
        if (held.has_value() && held->id == stack.id && held->components == stack.components) {
            return true;
        }
    }
    return false;
}

/// parity: `ItemUtils.createFilledResult`.
void fillResult(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                const StoredItemStack& held, const StoredItemStack& result, bool limitCreative = true) {
    const bool creative = player.gamemode == Gamemode::Creative;
    if (creative && limitCreative) {
        if (!carries(player, result)) {
            static_cast<void>(player.inventory.add(result));
        }
    } else if (!creative && held.count <= 1) {
        player.inventory.set(slot, result);
    } else {
        if (!creative) {
            StoredItemStack left = held;
            left.count -= 1;
            player.inventory.set(slot, left);
        }
        if (const std::int32_t leftover = player.inventory.add(result); leftover > 0) {
            if (const auto id = Identifier::parse(result.id); id.has_value()) {
                spawnDroppedItem(context, world, *id, leftover, player.x, player.y + 1.32, player.z, 0.0, 0.0, 0.0, 40,
                                 result.components);
            }
        }
    }
    if (player.connection().sendInventoryAll) {
        player.connection().sendInventoryAll();
    }
}

void setCauldron(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t stateId, std::string_view event) {
    if (!world.state().setBlockState(x, y, z, stateId)) {
        return;
    }
    announceBlock(context, world.name(), x, y, z, stateId);
    notifyAround(context, world, x, y, z, std::nullopt);
    heardBySculk(context, world, event, x, y, z);
}

void playSound(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId, std::string sound) {
    if (context.blocks != nullptr) {
        context.blocks->sendEffects(
            playersIn(context, world.name()), x, y, z, stateId,
            {world::blockEffect::Sound{.name = std::move(sound), .source = wire::protocol::play::SoundSource::Blocks}});
    }
}

[[nodiscard]] std::int32_t stateOf(std::string_view block, std::optional<std::int32_t> level = std::nullopt) {
    const Identifier id = Identifier::vanilla(std::string{block});
    if (level.has_value()) {
        return reg::blockStateDefinitions::stateIdOf(id, {{"level", std::to_string(*level)}}).value_or(0);
    }
    return reg::blocks::defaultStateIdOf(id).value_or(0);
}

/// parity: `LayeredCauldronBlock.lowerFillLevel`, down one, and empty at nothing.
void lowerFillLevel(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                    std::int32_t z, std::string_view block, std::int32_t level) {
    setCauldron(context, world, x, y, z, level <= 1 ? stateOf("cauldron") : stateOf(block, level - 1), "block_change");
}

/// parity: `CauldronInteractions.isUnderWater`.
[[nodiscard]] bool underWater(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> above = world.state().blockState(x, y + 1, z);
    return above.has_value() && world::fluids::of(*above) == world::Submersion::Water;
}

/// parity: `emptyBucket`: the cauldron takes what the bucket held.
void emptyBucket(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                 std::int32_t y, std::int32_t z, std::int32_t slot, const StoredItemStack& held, std::int32_t cauldron,
                 std::string sound) {
    fillResult(context, player, world, slot, held, StoredItemStack{.id = "minecraft:bucket", .count = 1});
    statAwards::award(player, "fill_cauldron");
    statAwards::used(player, held.id);
    setCauldron(context, world, x, y, z, cauldron, "fluid_place");
    playSound(context, world, x, y, z, cauldron, std::move(sound));
}

/// parity: `fillBucket`.
void fillBucket(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                std::int32_t y, std::int32_t z, std::int32_t slot, const StoredItemStack& held, std::string filled,
                std::string sound) {
    fillResult(context, player, world, slot, held, StoredItemStack{.id = std::move(filled), .count = 1});
    statAwards::award(player, "use_cauldron");
    statAwards::used(player, held.id);
    const std::int32_t empty = stateOf("cauldron");
    setCauldron(context, world, x, y, z, empty, "fluid_pickup");
    playSound(context, world, x, y, z, empty, std::move(sound));
}

} // namespace

bool useCauldronFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                    std::int32_t y, std::int32_t z, std::int32_t heldSlot, const std::optional<StoredItemStack>& held) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const Identifier* block = stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
    const std::optional<Contents> contents = block == nullptr ? std::nullopt : contentsOf(block->path());
    if (!contents.has_value() || !held.has_value()) {
        return false;
    }
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*stateId);
    const std::int32_t level = properties.contains("level") ? std::stoi(properties.at("level")) : 0;
    const std::string& item = held->id;
    // parity: `addDefaultInteractions`, which every kind of cauldron has.
    if (item == "minecraft:water_bucket") {
        emptyBucket(context, player, world, x, y, z, heldSlot, *held, stateOf("water_cauldron", kFullLevel),
                    "item.bucket.empty");
        return true;
    }
    if (item == "minecraft:lava_bucket" || item == "minecraft:powder_snow_bucket") {
        // parity: lava and powder snow answer `CONSUME` under water and do nothing.
        if (!underWater(world, x, y, z)) {
            const bool lava = item == "minecraft:lava_bucket";
            emptyBucket(context, player, world, x, y, z, heldSlot, *held,
                        lava ? stateOf("lava_cauldron") : stateOf("powder_snow_cauldron", kFullLevel),
                        lava ? "item.bucket.empty_lava" : "item.bucket.empty_powder_snow");
        }
        return true;
    }
    switch (*contents) {
    case Contents::Empty:
        if (isWaterPotion(*held)) {
            fillResult(context, player, world, heldSlot, *held,
                       StoredItemStack{.id = "minecraft:glass_bottle", .count = 1});
            statAwards::award(player, "use_cauldron");
            statAwards::used(player, item);
            const std::int32_t water = stateOf("water_cauldron");
            setCauldron(context, world, x, y, z, water, "fluid_place");
            playSound(context, world, x, y, z, water, "item.bottle.empty");
            return true;
        }
        return false;
    case Contents::Lava:
        if (item == "minecraft:bucket") {
            fillBucket(context, player, world, x, y, z, heldSlot, *held, "minecraft:lava_bucket",
                       "item.bucket.fill_lava");
            return true;
        }
        return false;
    case Contents::PowderSnow:
        if (item == "minecraft:bucket" && level == kFullLevel) {
            fillBucket(context, player, world, x, y, z, heldSlot, *held, "minecraft:powder_snow_bucket",
                       "item.bucket.fill_powder_snow");
            return true;
        }
        return false;
    case Contents::Water:
        break;
    }
    if (item == "minecraft:bucket") {
        if (level != kFullLevel) {
            return false;
        }
        fillBucket(context, player, world, x, y, z, heldSlot, *held, "minecraft:water_bucket", "item.bucket.fill");
        return true;
    }
    if (item == "minecraft:glass_bottle") {
        fillResult(context, player, world, heldSlot, *held, waterPotion());
        statAwards::award(player, "use_cauldron");
        statAwards::used(player, item);
        lowerFillLevel(context, world, x, y, z, "water_cauldron", level);
        playSound(context, world, x, y, z, *stateId, "item.bottle.fill");
        heardBySculk(context, world, "fluid_pickup", x, y, z);
        return true;
    }
    if (isWaterPotion(*held)) {
        if (level == kFullLevel) {
            return false;
        }
        fillResult(context, player, world, heldSlot, *held,
                   StoredItemStack{.id = "minecraft:glass_bottle", .count = 1});
        statAwards::award(player, "use_cauldron");
        statAwards::used(player, item);
        const std::int32_t higher = stateOf("water_cauldron", level + 1);
        setCauldron(context, world, x, y, z, higher, "fluid_place");
        playSound(context, world, x, y, z, higher, "item.bottle.empty");
        return true;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(item);
    if (!id.has_value()) {
        return false;
    }
    // parity: `dyedItemIteration`, the colour washed off in the hand itself.
    if (context.itemTags != nullptr &&
        world::realItemTags::contains(*context.itemTags, "minecraft:cauldron_can_remove_dye", *id)) {
        if (!held->components.contains("minecraft:dyed_color")) {
            return false;
        }
        StoredItemStack washed = *held;
        static_cast<void>(washed.components.remove("minecraft:dyed_color"));
        player.inventory.set(heldSlot, washed);
        if (context.slotChanged) {
            context.slotChanged(player, heldSlot);
        }
        statAwards::award(player, "clean_armor");
        lowerFillLevel(context, world, x, y, z, "water_cauldron", level);
        return true;
    }
    // parity: `bannerInteraction`, the top pattern layer washed off a copy of one.
    if (id->path().ends_with("_banner")) {
        const wire::nbt::NbtList* layers = held->components.getList("minecraft:banner_patterns");
        if (layers == nullptr || layers->empty()) {
            return false;
        }
        StoredItemStack washed = *held;
        washed.count = 1;
        std::vector<wire::nbt::NbtTag> kept{layers->begin(), layers->end() - 1};
        if (kept.empty()) {
            static_cast<void>(washed.components.remove("minecraft:banner_patterns"));
        } else {
            washed.components.set("minecraft:banner_patterns", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{
                                                                   layers->elementType(), std::move(kept)}));
        }
        fillResult(context, player, world, heldSlot, *held, washed, false);
        statAwards::award(player, "clean_banner");
        lowerFillLevel(context, world, x, y, z, "water_cauldron", level);
        return true;
    }
    // parity: `shulkerBoxInteraction`.
    if (id->path().ends_with("_shulker_box")) {
        StoredItemStack washed = *held;
        washed.id = "minecraft:shulker_box";
        washed.count = 1;
        fillResult(context, player, world, heldSlot, *held, washed, false);
        statAwards::award(player, "clean_shulker_box");
        lowerFillLevel(context, world, x, y, z, "water_cauldron", level);
        return true;
    }
    return false;
}

} // namespace basalt::server::runtime::player
