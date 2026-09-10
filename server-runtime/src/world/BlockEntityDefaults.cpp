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
    return data;
}

} // namespace basalt::server::runtime::world::blockEntityDefaults
