#include <basalt/server/runtime/registry/VanillaDimensionTypes.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime::registry::vanillaDimensionTypes {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

[[nodiscard]] NbtCompound uniformInt(std::int32_t min, std::int32_t max) {
    NbtCompound value;
    value.putString("type", "minecraft:uniform");
    value.putInt("max_inclusive", max);
    value.putInt("min_inclusive", min);
    return value;
}

[[nodiscard]] NbtCompound music(std::string_view sound, std::int32_t minDelay, std::int32_t maxDelay,
                                bool replace = false) {
    NbtCompound value;
    value.putInt("max_delay", maxDelay);
    value.putInt("min_delay", minDelay);
    if (replace) {
        value.putBoolean("replace_current_music", true);
    }
    value.putString("sound", std::string{sound});
    return value;
}

[[nodiscard]] NbtCompound bedRule(std::string_view canSetSpawn, std::string_view canSleep, bool explodes) {
    NbtCompound value;
    value.putString("can_set_spawn", std::string{canSetSpawn});
    value.putString("can_sleep", std::string{canSleep});
    if (explodes) {
        value.putBoolean("explodes", true);
    }
    return value;
}

[[nodiscard]] NbtCompound ambientCaveSounds() {
    NbtCompound mood;
    mood.putInt("block_search_extent", 8);
    mood.putDouble("offset", 2.0);
    mood.putString("sound", "minecraft:ambient.cave");
    mood.putInt("tick_delay", 6000);
    NbtCompound value;
    value.set("mood", NbtTag::ofCompound(std::move(mood)));
    return value;
}

[[nodiscard]] NbtCompound overworldAttributes() {
    NbtCompound value;
    value.set("minecraft:audio/ambient_sounds", NbtTag::ofCompound(ambientCaveSounds()));
    NbtCompound backgroundMusic;
    backgroundMusic.set("creative", NbtTag::ofCompound(music("minecraft:music.creative", 12000, 24000)));
    backgroundMusic.set("default", NbtTag::ofCompound(music("minecraft:music.game", 12000, 24000)));
    value.set("minecraft:audio/background_music", NbtTag::ofCompound(std::move(backgroundMusic)));

    NbtCompound bed = bedRule("always", "when_dark", false);
    NbtCompound errorMessage;
    errorMessage.putString("translate", "block.minecraft.bed.no_sleep");
    bed.set("error_message", NbtTag::ofCompound(std::move(errorMessage)));
    value.set("minecraft:gameplay/bed_rule", NbtTag::ofCompound(std::move(bed)));

    value.putBoolean("minecraft:gameplay/nether_portal_spawns_piglin", true);
    value.putBoolean("minecraft:gameplay/respawn_anchor_works", false);
    value.putString("minecraft:visual/ambient_light_color", "#0a0a0a");
    value.putString("minecraft:visual/cloud_color", "#ccffffff");
    value.putFloat("minecraft:visual/cloud_height", 192.33F);
    value.putString("minecraft:visual/fog_color", "#c0d8ff");
    value.putString("minecraft:visual/sky_color", "#78a7ff");
    return value;
}

[[nodiscard]] NbtCompound netherAttributes() {
    NbtCompound value;
    value.set("minecraft:gameplay/bed_rule", NbtTag::ofCompound(bedRule("never", "never", true)));
    value.putBoolean("minecraft:gameplay/can_start_raid", false);
    value.putBoolean("minecraft:gameplay/fast_lava", true);
    value.putBoolean("minecraft:gameplay/piglins_zombify", false);
    value.putBoolean("minecraft:gameplay/respawn_anchor_works", true);
    value.putFloat("minecraft:gameplay/sky_light_level", 4.0F);
    value.putBoolean("minecraft:gameplay/snow_golem_melts", true);
    value.putBoolean("minecraft:gameplay/water_evaporates", true);
    value.putString("minecraft:visual/ambient_light_color", "#302821");
    NbtCompound dripstone;
    dripstone.putString("type", "minecraft:dripping_dripstone_lava");
    value.set("minecraft:visual/default_dripstone_particle", NbtTag::ofCompound(std::move(dripstone)));
    value.putFloat("minecraft:visual/fog_end_distance", 96.0F);
    value.putFloat("minecraft:visual/fog_start_distance", 10.0F);
    value.putString("minecraft:visual/sky_light_color", "#7a7aff");
    value.putFloat("minecraft:visual/sky_light_factor", 0.0F);
    return value;
}

[[nodiscard]] NbtCompound endAttributes() {
    NbtCompound value;
    value.set("minecraft:audio/ambient_sounds", NbtTag::ofCompound(ambientCaveSounds()));
    NbtCompound backgroundMusic;
    backgroundMusic.set("default", NbtTag::ofCompound(music("minecraft:music.end", 6000, 24000, true)));
    value.set("minecraft:audio/background_music", NbtTag::ofCompound(std::move(backgroundMusic)));
    value.set("minecraft:gameplay/bed_rule", NbtTag::ofCompound(bedRule("never", "never", true)));
    value.putBoolean("minecraft:gameplay/respawn_anchor_works", false);
    value.putString("minecraft:visual/ambient_light_color", "#3f473f");
    value.putString("minecraft:visual/fog_color", "#181318");
    value.putString("minecraft:visual/sky_color", "#000000");
    value.putString("minecraft:visual/sky_light_color", "#ac60cd");
    value.putFloat("minecraft:visual/sky_light_factor", 0.0F);
    return value;
}

struct DimensionShape {
    bool hasFixedTime;
    bool hasSkylight;
    bool hasCeiling;
    bool hasEnderDragonFight;
    double coordinateScale;
    std::int32_t minY;
    std::int32_t height;
    std::int32_t logicalHeight;
    std::string_view infiniburn;
    float ambientLight;
    NbtTag monsterSpawnLightLevel;
    std::int32_t monsterSpawnBlockLightLimit;
    NbtCompound attributes;
};

[[nodiscard]] NbtCompound dimensionType(DimensionShape shape) {
    NbtCompound value;
    // Written only when true, the way the codec's default leaves it out.
    if (shape.hasFixedTime) {
        value.putBoolean("has_fixed_time", true);
    }
    value.putBoolean("has_skylight", shape.hasSkylight);
    value.putBoolean("has_ceiling", shape.hasCeiling);
    value.putBoolean("has_ender_dragon_fight", shape.hasEnderDragonFight);
    value.putDouble("coordinate_scale", shape.coordinateScale);
    value.putInt("min_y", shape.minY);
    value.putInt("height", shape.height);
    value.putInt("logical_height", shape.logicalHeight);
    value.putString("infiniburn", std::string{shape.infiniburn});
    value.putFloat("ambient_light", shape.ambientLight);
    value.set("monster_spawn_light_level", std::move(shape.monsterSpawnLightLevel));
    value.putInt("monster_spawn_block_light_limit", shape.monsterSpawnBlockLightLimit);
    value.set("attributes", NbtTag::ofCompound(std::move(shape.attributes)));
    return value;
}

[[nodiscard]] NbtCompound overworldDimensionType(bool hasCeiling) {
    NbtCompound value = dimensionType(DimensionShape{.hasFixedTime = false,
                                                     .hasSkylight = true,
                                                     .hasCeiling = hasCeiling,
                                                     .hasEnderDragonFight = false,
                                                     .coordinateScale = 1.0,
                                                     .minY = -64,
                                                     .height = 384,
                                                     .logicalHeight = 384,
                                                     .infiniburn = "#minecraft:infiniburn_overworld",
                                                     .ambientLight = 0.0F,
                                                     .monsterSpawnLightLevel = NbtTag::ofCompound(uniformInt(0, 7)),
                                                     .monsterSpawnBlockLightLimit = 0,
                                                     .attributes = overworldAttributes()});
    value.putString("default_clock", "minecraft:overworld");
    value.putString("timelines", "#minecraft:in_overworld");
    return value;
}

[[nodiscard]] NbtCompound netherDimensionType() {
    NbtCompound value = dimensionType(DimensionShape{.hasFixedTime = true,
                                                     .hasSkylight = false,
                                                     .hasCeiling = true,
                                                     .hasEnderDragonFight = false,
                                                     .coordinateScale = 8.0,
                                                     .minY = 0,
                                                     .height = 256,
                                                     .logicalHeight = 128,
                                                     .infiniburn = "#minecraft:infiniburn_nether",
                                                     .ambientLight = 0.1F,
                                                     .monsterSpawnLightLevel = NbtTag::ofInt(7),
                                                     .monsterSpawnBlockLightLimit = 15,
                                                     .attributes = netherAttributes()});
    value.putString("cardinal_light", "nether");
    value.putString("skybox", "none");
    value.putString("timelines", "#minecraft:in_nether");
    return value;
}

[[nodiscard]] NbtCompound endDimensionType() {
    NbtCompound value = dimensionType(DimensionShape{.hasFixedTime = true,
                                                     .hasSkylight = true,
                                                     .hasCeiling = false,
                                                     .hasEnderDragonFight = true,
                                                     .coordinateScale = 1.0,
                                                     .minY = 0,
                                                     .height = 256,
                                                     .logicalHeight = 256,
                                                     .infiniburn = "#minecraft:infiniburn_end",
                                                     .ambientLight = 0.25F,
                                                     .monsterSpawnLightLevel = NbtTag::ofInt(15),
                                                     .monsterSpawnBlockLightLimit = 0,
                                                     .attributes = endAttributes()});
    value.putString("default_clock", "minecraft:the_end");
    value.putString("skybox", "end");
    value.putString("timelines", "#minecraft:in_end");
    return value;
}

} // namespace

const std::vector<std::pair<std::string, NbtCompound>>& worldClocks() {
    static const std::vector<std::pair<std::string, NbtCompound>> built{{"overworld", NbtCompound{}},
                                                                        {"the_end", NbtCompound{}}};
    return built;
}

const std::vector<std::pair<std::string, NbtCompound>>& dimensionTypes() {
    static const std::vector<std::pair<std::string, NbtCompound>> built{
        {"overworld", overworldDimensionType(false)},
        {"the_nether", netherDimensionType()},
        {"the_end", endDimensionType()},
        {"overworld_caves", overworldDimensionType(true)}};
    return built;
}

NbtCompound overworld() { return overworldDimensionType(false); }

} // namespace basalt::server::runtime::registry::vanillaDimensionTypes
