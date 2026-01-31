#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>

#include <string_view>

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
        data.putInt("TransferCooldown", 0);
        return data;
    }
    // parity: `ComparatorBlockEntity.saveAdditional`, one line and always written.
    if (path == "comparator") {
        data.putInt("OutputSignal", 0);
        return data;
    }
    // parity: `SculkSensorBlockEntity.saveAdditional`. The `listener` beside it is the vibration
    // queue's own bookkeeping, which this build does not model and vanilla fills in on read.
    if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
        data.putInt("last_vibration_frequency", 0);
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
    return data;
}

} // namespace basalt::server::runtime::world::blockEntityDefaults
