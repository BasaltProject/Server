#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BaseSpawnerData.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>

#include <cctype>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::blockEntityDefaults {
namespace {

[[nodiscard]] std::string modeOf(std::optional<std::int32_t> stateId, std::string_view fallback) {
    if (!stateId.has_value()) {
        return std::string{fallback};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto mode = properties.find("mode");
    return mode == properties.end() ? std::string{fallback} : mode->second;
}

[[nodiscard]] std::string upperModeOf(std::optional<std::int32_t> stateId, std::string_view fallback) {
    std::string mode = modeOf(stateId, fallback);
    for (char& letter : mode) {
        letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    }
    return mode;
}

[[nodiscard]] std::string lowerModeOf(std::optional<std::int32_t> stateId, std::string_view fallback) {
    return modeOf(stateId, fallback);
}

} // namespace

wire::nbt::NbtCompound of(const wire::common::Identifier& block, std::optional<std::int32_t> stateId) {
    const std::string_view path = block.path();
    // parity: `StructureBlockEntity.saveAdditional`.
    if (path == "structure_block") {
        wire::nbt::NbtCompound structure;
        structure.putString("name", "");
        structure.putString("author", "");
        structure.putString("metadata", "");
        structure.putInt("posX", 0);
        structure.putInt("posY", 1);
        structure.putInt("posZ", 0);
        structure.putInt("sizeX", 0);
        structure.putInt("sizeY", 0);
        structure.putInt("sizeZ", 0);
        structure.putString("rotation", "NONE");
        structure.putString("mirror", "NONE");
        structure.putString("mode", upperModeOf(stateId, "DATA"));
        structure.putBoolean("ignoreEntities", true);
        structure.putBoolean("strict", false);
        structure.putBoolean("powered", false);
        structure.putBoolean("showair", false);
        structure.putBoolean("showboundingbox", true);
        structure.putFloat("integrity", 1.0F);
        structure.putLong("seed", 0);
        return structure;
    }
    // parity: `TestBlockEntity.saveAdditional`.
    if (path == "test_block") {
        wire::nbt::NbtCompound test;
        test.putString("mode", lowerModeOf(stateId, "start"));
        test.putString("message", "");
        test.putBoolean("powered", false);
        return test;
    }
    if (commandBlocks::modeOf(path).has_value()) {
        return commandBlocks::defaults();
    }
    wire::nbt::NbtCompound data;
    // parity: `AbstractFurnaceBlockEntity.saveAdditional`.
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
    // parity: `HopperBlockEntity.saveAdditional`.
    if (path == "hopper") {
        // parity: `cooldownTime` starts at **minus one**.
        data.putInt("TransferCooldown", -1);
        return data;
    }
    // parity: `ComparatorBlockEntity.saveAdditional`, one line and always written.
    if (path == "comparator") {
        data.putInt("OutputSignal", 0);
        return data;
    }
    // parity: `SculkSensorBlockEntity.saveAdditional`.
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
    // parity: `CampfireBlockEntity.saveAdditional`.
    if (campfires::isCampfire(path)) {
        data.putIntArray("CookingTimes", std::vector<std::int32_t>(campfires::kSize, 0));
        data.putIntArray("CookingTotalTimes", std::vector<std::int32_t>(campfires::kSize, 0));
        return data;
    }
    // parity: `CrafterBlockEntity.saveAdditional` plus its two helpers.
    if (path == "crafter") {
        data.putInt("crafting_ticks_remaining", 0);
        data.putIntArray("disabled_slots", {});
        data.putInt("triggered", 0);
        return data;
    }
    // parity: `BeehiveBlockEntity.saveAdditional`.
    if (path == "beehive" || path == "bee_nest") {
        data.set("bees", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, {}}));
        return data;
    }
    // parity: `ChiseledBookShelfBlockEntity.saveAdditional`.
    if (path == "chiseled_bookshelf") {
        data.putInt("last_interacted_slot", -1);
        return data;
    }
    // parity: `PotentSulfurBlockEntity.saveAdditional`.
    if (path == "potent_sulfur") {
        data.putInt("countdown", -1);
        return data;
    }
    // parity: `ShelfBlockEntity.saveAdditional`.
    if (shelves::isShelf(path)) {
        data.putBoolean("align_items_to_bottom", false);
        return data;
    }
    // parity: `BaseSpawner.save` of a spawner nobody has set.
    if (path == "spawner") {
        baseSpawnerData::save(BaseSpawnerState{}, data);
        return data;
    }
    return data;
}

} // namespace basalt::server::runtime::world::blockEntityDefaults
