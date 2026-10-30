#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/registry/VanillaDimensionTypes.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/registry/VanillaTagData.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::registry;

namespace {

[[nodiscard]] std::int64_t checksumOf(std::string_view text) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const char character : text) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    return static_cast<std::int64_t>(checksum);
}

} // namespace

TEST_CASE("the tag table goes out in a fixed order, each registry's names sorted and distinct") {
    std::string order;
    for (const auto& [name, tags] : vanillaTagData::tagsByRegistry()) {
        if (!order.empty()) {
            order += ',';
        }
        order += name;
        CAPTURE(name);
        CHECK(std::ranges::is_sorted(tags));
        CHECK(std::ranges::adjacent_find(tags) == tags.end());
    }
    CHECK(order == "minecraft:banner_pattern,minecraft:block,minecraft:damage_type,minecraft:dialog,"
                   "minecraft:enchantment,minecraft:entity_type,minecraft:fluid,minecraft:game_event,"
                   "minecraft:instrument,minecraft:item,minecraft:painting_variant,"
                   "minecraft:point_of_interest_type,minecraft:potion,minecraft:timeline,"
                   "minecraft:worldgen/biome");

    CHECK(vanillaTagData::tagsOf("minecraft:worldgen/world_preset").empty());
    CHECK(vanillaTagData::tagsOf("").empty());
}

TEST_CASE("the variant registries are the lists the frozen kotlin source spells out") {
    struct Expected {
        std::string_view name;
        const std::vector<std::string>& values;
        std::size_t count;
        std::string_view first;
        std::string_view last;
    };

    const Expected kExpected[] = {
        {"bannerPatterns", vanillaRegistryData::bannerPatterns(), 43, "base", "guster"},
        {"wolfVariants", vanillaRegistryData::wolfVariants(), 9, "pale", "striped"},
        {"wolfSoundVariants", vanillaRegistryData::wolfSoundVariants(), 7, "classic", "cute"},
        {"pigVariants", vanillaRegistryData::pigVariants(), 3, "temperate", "cold"},
        {"pigSoundVariants", vanillaRegistryData::pigSoundVariants(), 3, "classic", "mini"},
        {"frogVariants", vanillaRegistryData::frogVariants(), 3, "temperate", "cold"},
        {"catVariants", vanillaRegistryData::catVariants(), 11, "tabby", "all_black"},
        {"catSoundVariants", vanillaRegistryData::catSoundVariants(), 2, "classic", "royal"},
        {"cowVariants", vanillaRegistryData::cowVariants(), 3, "temperate", "cold"},
        {"cowSoundVariants", vanillaRegistryData::cowSoundVariants(), 2, "classic", "moody"},
        {"chickenVariants", vanillaRegistryData::chickenVariants(), 3, "temperate", "cold"},
        {"chickenSoundVariants", vanillaRegistryData::chickenSoundVariants(), 2, "classic", "picky"},
        {"zombieNautilusVariants", vanillaRegistryData::zombieNautilusVariants(), 2, "temperate", "warm"},
    };
    for (const Expected& expected : kExpected) {
        CAPTURE(expected.name);
        CHECK(expected.values.size() == expected.count);
        REQUIRE_FALSE(expected.values.empty());
        CHECK(expected.values.front() == expected.first);
        CHECK(expected.values.back() == expected.last);
    }

    CHECK(vanillaRegistryData::catVariants()[10] == "all_black");
    CHECK(vanillaRegistryData::bannerPatterns()[0] == "base");

    CHECK(vanillaRegistryData::trimMaterials().size() == 11);
    CHECK(vanillaRegistryData::trimMaterials().front().first == "quartz");
    CHECK(vanillaRegistryData::trimMaterials().front().second == 14931140);
    CHECK(vanillaRegistryData::trimMaterials().back().first == "resin");
    CHECK(vanillaRegistryData::damageTypes().size() == 51);
    CHECK(vanillaRegistryData::damageTypes().front().first == "in_fire");
    CHECK(vanillaRegistryData::damageTypes().back().first == "mace_smash");
    CHECK(vanillaRegistryData::jukeboxSongs().size() == 22);
    CHECK(vanillaRegistryData::jukeboxSongs().front().first == "13");
    CHECK(vanillaRegistryData::jukeboxSongs().back().first == "tears");
    CHECK(vanillaRegistryData::instruments().size() == 8);
    CHECK(vanillaRegistryData::instruments().front().first == "ponder_goat_horn");
    CHECK(vanillaRegistryData::instruments().back().first == "dream_goat_horn");

    std::string joined;
    const auto append = [&joined](std::string_view name, const std::vector<std::string>& values) {
        if (!joined.empty()) {
            joined += ';';
        }
        joined += std::string{name} + "=";
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index != 0) {
                joined += ',';
            }
            joined += values[index];
        }
    };
    for (const Expected& expected : kExpected) {
        append(expected.name, expected.values);
    }
    const auto appendKeys = [&joined](std::string_view name, const auto& entries) {
        joined += ";" + std::string{name} + "=";
        for (std::size_t index = 0; index < entries.size(); ++index) {
            if (index != 0) {
                joined += ',';
            }
            joined += entries[index].first;
        }
    };
    appendKeys("trimMaterials", vanillaRegistryData::trimMaterials());
    appendKeys("damageTypes", vanillaRegistryData::damageTypes());
    appendKeys("jukeboxSongs", vanillaRegistryData::jukeboxSongs());
    appendKeys("instruments", vanillaRegistryData::instruments());
    CHECK(joined.size() == 2048);
    CHECK(checksumOf(joined) == 7056471522420214086LL);
}

TEST_CASE("a variant's own entry is built the way the codec reads it") {
    namespace nbt = basalt::wire::nbt;
    const auto text = [](const nbt::NbtCompound& value, std::string_view key) {
        const nbt::NbtTag* found = value.find(key);
        if (found == nullptr) {
            return std::string{"-"};
        }
        const std::string* written = found->get<std::string>();
        return written == nullptr ? std::string{"-"} : *written;
    };

    const nbt::NbtCompound pale = vanillaRegistryData::wolfVariant("pale");
    CHECK(text(*pale.find("assets")->get<nbt::NbtCompound>(), "wild") == "minecraft:entity/wolf/wolf");
    CHECK(text(*pale.find("baby_assets")->get<nbt::NbtCompound>(), "angry") == "minecraft:entity/wolf/wolf_angry_baby");
    const nbt::NbtCompound ashen = vanillaRegistryData::wolfVariant("ashen");
    CHECK(text(*ashen.find("assets")->get<nbt::NbtCompound>(), "tame") == "minecraft:entity/wolf/wolf_ashen_tame");

    const nbt::NbtCompound grumpy = vanillaRegistryData::wolfSoundVariant("grumpy");
    const nbt::NbtCompound& adult = *grumpy.find("adult_sounds")->get<nbt::NbtCompound>();
    CHECK(text(adult, "ambient_sound") == "minecraft:entity.wolf_grumpy.ambient");
    CHECK(text(adult, "step_sound") == "minecraft:entity.wolf.step");
    CHECK(text(*grumpy.find("baby_sounds")->get<nbt::NbtCompound>(), "whine_sound") ==
          "minecraft:entity.baby_wolf.whine");

    CHECK(text(vanillaRegistryData::pigVariant("warm"), "asset_id") == "minecraft:entity/pig/pig_warm");
    CHECK(text(vanillaRegistryData::pigVariant("warm"), "baby_asset_id") == "minecraft:entity/pig/pig_warm_baby");
    CHECK(text(vanillaRegistryData::catVariant("jellie"), "baby_asset_id") == "minecraft:entity/cat/cat_jellie_baby");
    CHECK(text(vanillaRegistryData::cowVariant("cold"), "baby_asset_id") == "minecraft:entity/cow/cow_cold_baby");
    CHECK(text(vanillaRegistryData::chickenVariant("temperate"), "baby_asset_id") ==
          "minecraft:entity/chicken/chicken_temperate_baby");
    CHECK(text(vanillaRegistryData::frogVariant("warm"), "asset_id") == "minecraft:entity/frog/frog_warm");
    CHECK(vanillaRegistryData::frogVariant("warm").find("baby_asset_id") == nullptr);

    CHECK(text(vanillaRegistryData::pigVariant("cold"), "model") == "cold");
    CHECK(text(vanillaRegistryData::pigVariant("warm"), "model") == "-");
    CHECK(text(vanillaRegistryData::cowVariant("warm"), "model") == "warm");
    CHECK(text(vanillaRegistryData::cowVariant("temperate"), "model") == "-");
    CHECK(text(vanillaRegistryData::zombieNautilusVariant("warm"), "asset_id") ==
          "minecraft:entity/nautilus/zombie_nautilus_coral");
    CHECK(text(vanillaRegistryData::zombieNautilusVariant("temperate"), "asset_id") ==
          "minecraft:entity/nautilus/zombie_nautilus");

    const nbt::NbtCompound moody = vanillaRegistryData::cowSoundVariant("moody");
    CHECK(moody.find("adult_sounds") == nullptr);
    CHECK(text(moody, "step_sound") == "minecraft:entity.cow_moody.step");
    CHECK(text(vanillaRegistryData::cowSoundVariant("classic"), "ambient_sound") == "minecraft:entity.cow.ambient");

    CHECK(text(vanillaRegistryData::bannerPattern("globe"), "asset_id") == "minecraft:globe");
    CHECK(text(vanillaRegistryData::bannerPattern("globe"), "translation_key") == "block.minecraft.banner.globe");
    CHECK(text(vanillaRegistryData::trimMaterial("iron", 15527148), "palette_id") == "minecraft:trim/iron");
}

TEST_CASE("the dimension types carry what a client joins a world with") {
    namespace nbt = basalt::wire::nbt;
    const auto& types = vanillaDimensionTypes::dimensionTypes();
    REQUIRE(types.size() == 4);
    CHECK(types[0].first == "overworld");
    CHECK(types[1].first == "the_nether");
    CHECK(types[2].first == "the_end");
    CHECK(types[3].first == "overworld_caves");

    const nbt::NbtCompound& overworld = types[0].second;
    CHECK(overworld.find("has_fixed_time") == nullptr);
    CHECK(overworld.getBoolean("has_skylight"));
    CHECK_FALSE(overworld.getBoolean("has_ceiling"));
    CHECK(overworld.getInt("min_y") == -64);
    CHECK(overworld.getInt("height") == 384);
    CHECK(overworld.getDouble("coordinate_scale") == 1.0);
    CHECK(overworld.find("monster_spawn_light_level")->get<nbt::NbtCompound>() != nullptr);

    const nbt::NbtCompound& nether = types[1].second;
    CHECK(nether.getBoolean("has_fixed_time"));
    CHECK(nether.getDouble("coordinate_scale") == 8.0);
    CHECK(nether.getInt("logical_height") == 128);
    CHECK(*nether.find("monster_spawn_light_level")->get<std::int32_t>() == 7);
    CHECK(nether.getString("skybox") == "none");

    const nbt::NbtCompound& end = types[2].second;
    CHECK(end.getBoolean("has_ender_dragon_fight"));
    CHECK(end.getString("default_clock") == "minecraft:the_end");

    CHECK(types[3].second.getBoolean("has_ceiling"));
    CHECK(types[3].second.getInt("min_y") == -64);

    REQUIRE(vanillaDimensionTypes::worldClocks().size() == 2);
    CHECK(vanillaDimensionTypes::worldClocks()[0].first == "overworld");
    CHECK(vanillaDimensionTypes::worldClocks()[0].second.empty());
    CHECK(vanillaDimensionTypes::worldClocks()[1].first == "the_end");
}
