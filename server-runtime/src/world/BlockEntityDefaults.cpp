#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>

#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::blockEntityDefaults {

wire::nbt::NbtCompound of(const wire::common::Identifier& block) {
    const std::string_view path = block.path();
    if (commandBlocks::modeOf(path).has_value()) {
        return commandBlocks::defaults();
    }
    wire::nbt::NbtCompound data;
    // parity: `AbstractFurnaceBlockEntity.saveAdditional`, four shorts and the book of recipes it
    // owes experience for, all written every time whatever they hold.
    if (furnaces::cookerFor(path).has_value()) {
        data.putShort("cooking_time_spent", 0);
        data.putShort("cooking_total_time", 0);
        data.putShort("lit_time_remaining", 0);
        data.putShort("lit_total_time", 0);
        data.set("RecipesUsed", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
        return data;
    }
    // parity: `BrewingStandBlockEntity.saveAdditional`: a short and a byte, in that order.
    if (path == "brewing_stand") {
        data.putShort("BrewTime", 0);
        data.putByte("Fuel", 0);
        return data;
    }
    // parity: `HopperBlockEntity.saveAdditional`, which writes the cooldown outside the loot table
    // branch, so even an untouched hopper carries it.
    if (path == "hopper") {
        // parity: `cooldownTime` starts at **minus one**, not zero: `isOnCooldown` is `> 0`, so the
        // two behave the same, but a hopper nobody has touched saves a minus one.
        data.putInt("TransferCooldown", -1);
        return data;
    }
    // parity: `ComparatorBlockEntity.saveAdditional`, one line and always written.
    if (path == "comparator") {
        data.putInt("OutputSignal", 0);
        return data;
    }
    // parity: `SculkSensorBlockEntity.saveAdditional`, which stores `VibrationSystem.Data` under
    // `listener` **always**: the codec has `selector` as a required field and `event_delay` as one
    // that is always written, so a sensor nobody has shouted at still saves the empty shape. The
    // `tick` inside the selector is a long, and minus one is the codec's own "nothing chosen yet".
    if (path == "sculk_sensor" || path == "calibrated_sculk_sensor" || path == "sculk_shrieker") {
        if (path != "sculk_shrieker") {
            data.putInt("last_vibration_frequency", 0);
        } else {
            // parity: `SculkShriekerBlockEntity.saveAdditional`, which writes its warning level.
            data.putInt("warning_level", 0);
        }
        wire::nbt::NbtCompound selector;
        selector.putLong("tick", -1);
        wire::nbt::NbtCompound listener;
        listener.set("selector", wire::nbt::NbtTag::ofCompound(std::move(selector)));
        listener.putInt("event_delay", 0);
        data.set("listener", wire::nbt::NbtTag::ofCompound(std::move(listener)));
        return data;
    }
    // parity: `CampfireBlockEntity.saveAdditional`, which writes both arrays whatever is on the
    // fire: a campfire nobody has cooked on saves four zeros twice.
    if (campfires::isCampfire(path)) {
        data.putIntArray("CookingTimes", std::vector<std::int32_t>(campfires::kSize, 0));
        data.putIntArray("CookingTotalTimes", std::vector<std::int32_t>(campfires::kSize, 0));
        return data;
    }
    // parity: `CrafterBlockEntity.saveAdditional` plus its two helpers, three fields every time.
    if (path == "crafter") {
        data.putInt("crafting_ticks_remaining", 0);
        data.putIntArray("disabled_slots", {});
        data.putInt("triggered", 0);
        return data;
    }
    // parity: `BeehiveBlockEntity.saveAdditional`, whose list goes out even when nobody is home.
    if (path == "beehive" || path == "bee_nest") {
        data.set("bees", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, {}}));
        return data;
    }
    // parity: `ChiseledBookShelfBlockEntity.saveAdditional`, whose slot is -1 until somebody touches
    // one, which is what makes an untouched shelf read nothing on a comparator.
    if (path == "chiseled_bookshelf") {
        data.putInt("last_interacted_slot", -1);
        return data;
    }
    // parity: `ShelfBlockEntity.saveAdditional`, which writes how it lines its three items up even
    // for a shelf nobody has touched.
    if (shelves::isShelf(path)) {
        data.putBoolean("align_items_to_bottom", false);
        return data;
    }
    return data;
}

} // namespace basalt::server::runtime::world::blockEntityDefaults
