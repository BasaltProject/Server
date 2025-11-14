#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/registry/VanillaBiomes.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaOverworldBiomes.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace basalt::server::runtime::registry;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] std::int64_t checksumOf(std::string_view text) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const char character : text) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    return static_cast<std::int64_t>(checksum);
}

[[nodiscard]] std::string textAt(const nbt::NbtCompound& value, std::string_view key) {
    const nbt::NbtTag* found = value.find(key);
    if (found == nullptr) {
        return "-";
    }
    const std::string* written = found->get<std::string>();
    return written == nullptr ? "-" : *written;
}

} // namespace

/// This module's frozen Kotlin cannot be run, so the numbers below are derived from the same source
/// text the entries were transcribed from: the order, the counts of each attribute kind, and the
/// counts of each optional field.
TEST_CASE("the biome table is the one the frozen kotlin source spells out") {
    const std::vector<vanillaBiomes::Entry>& all = vanillaBiomes::all();
    CHECK(all.size() == 66);
    CHECK(vanillaBiomes::overworldBiomesA().size() == 29);
    CHECK(vanillaBiomes::overworldBiomesB().size() == 27);
    CHECK(vanillaBiomes::netherBiomes().size() == 5);
    CHECK(vanillaBiomes::endBiomes().size() == 5);

    std::string joined;
    for (const vanillaBiomes::Entry& entry : all) {
        if (!joined.empty()) {
            joined += ',';
        }
        joined += entry.path;
    }
    // The order is the network id a chunk's biome palette references, so it is pinned rather than
    // sorted: the two halves join at `wooded_badlands` into `meadow`, then the nether, then the end.
    CHECK(checksumOf(joined) == 9160950417039270003LL);
    CHECK(all.front().path == "plains");
    CHECK(all[28].path == "wooded_badlands");
    CHECK(all[29].path == "meadow");
    CHECK(all[56].path == "nether_wastes");
    CHECK(all[61].path == "the_end");
    CHECK(all.back().path == "end_barrens");

    // No two biomes answer to the same name, which is what makes the index an address.
    std::map<std::string, std::int32_t> byPath;
    for (const vanillaBiomes::Entry& entry : all) {
        ++byPath[entry.path];
    }
    CHECK(byPath.size() == 66);
}

TEST_CASE("every attribute kind and optional field appears as often as the source has it") {
    std::map<vanillaBiomes::BiomeAttributeKind, std::int32_t> kinds;
    std::int32_t withAttributes = 0;
    std::map<std::string, std::int32_t> optional;
    for (const vanillaBiomes::Entry& entry : vanillaBiomes::all()) {
        if (!entry.attributes.empty()) {
            ++withAttributes;
        }
        for (const auto& [key, attribute] : entry.attributes) {
            ++kinds[attribute.kind];
        }
        if (entry.temperatureModifier.has_value()) {
            ++optional["temperatureModifier"];
        }
        if (entry.grassColor.has_value()) {
            ++optional["grassColor"];
        }
        if (entry.foliageColor.has_value()) {
            ++optional["foliageColor"];
        }
        if (entry.dryFoliageColor.has_value()) {
            ++optional["dryFoliageColor"];
        }
        if (entry.grassColorModifier.has_value()) {
            ++optional["grassColorModifier"];
        }
    }

    CHECK(withAttributes == 61);
    CHECK(kinds[vanillaBiomes::BiomeAttributeKind::Color] == 72);
    CHECK(kinds[vanillaBiomes::BiomeAttributeKind::Flag] == 16);
    CHECK(kinds[vanillaBiomes::BiomeAttributeKind::Number] == 1);
    CHECK(kinds[vanillaBiomes::BiomeAttributeKind::Modifier] == 2);
    CHECK(kinds[vanillaBiomes::BiomeAttributeKind::Music] == 42);
    CHECK(kinds[vanillaBiomes::BiomeAttributeKind::Particles] == 4);
    CHECK(kinds[vanillaBiomes::BiomeAttributeKind::Sounds] == 5);

    CHECK(optional["temperatureModifier"] == 2);
    CHECK(optional["grassColor"] == 6);
    CHECK(optional["foliageColor"] == 7);
    CHECK(optional["dryFoliageColor"] == 4);
    CHECK(optional["grassColorModifier"] == 3);
}

TEST_CASE("one biome serializes to the exact nbt a vanilla server sends") {
    const auto find = [](std::string_view path) {
        for (const vanillaBiomes::Entry& entry : vanillaBiomes::all()) {
            if (entry.path == path) {
                return entry;
            }
        }
        FAIL("no such biome: ", path);
        return vanillaBiomes::Entry{};
    };

    // The plainest of them: nothing optional, one attribute.
    const nbt::NbtCompound plains = vanillaBiomes::toNbt(find("plains"));
    CHECK(nbt::snbt::stringify(nbt::NbtTag::ofCompound(plains)) ==
          R"({has_precipitation:1b,temperature:0.8f,downfall:0.4f,attributes:{"minecraft:visual/sky_color":)"
          R"("#78a7ff"},effects:{water_color:"#3f76e4"}})");

    // Every optional colour field it has, in the order the codec reads them: foliage, dry foliage,
    // grass, then the modifier.
    const nbt::NbtCompound swamp = vanillaBiomes::toNbt(find("swamp"));
    const nbt::NbtCompound& effects = *swamp.find("effects")->get<nbt::NbtCompound>();
    CHECK(effects.names().size() == 4);
    CHECK(effects.names()[0] == "water_color");
    CHECK(effects.names()[1] == "foliage_color");
    CHECK(effects.names()[2] == "dry_foliage_color");
    CHECK(effects.names()[3] == "grass_color_modifier");
    CHECK(textAt(effects, "grass_color_modifier") == "swamp");
    // The modifier attribute is an object, not a number.
    const nbt::NbtCompound& distance = *swamp.find("attributes")
                                            ->get<nbt::NbtCompound>()
                                            ->find("minecraft:visual/water_fog_end_distance")
                                            ->get<nbt::NbtCompound>();
    CHECK(*distance.find("argument")->get<float>() == 0.85F);
    CHECK(textAt(distance, "modifier") == "multiply");

    // `temperature_modifier` is written between the temperature and the downfall, which is where the
    // codec expects it, and only by the two biomes that have one.
    const nbt::NbtCompound frozen = vanillaBiomes::toNbt(find("frozen_ocean"));
    CHECK(frozen.names()[1] == "temperature");
    CHECK(frozen.names()[2] == "temperature_modifier");
    CHECK(frozen.names()[3] == "downfall");
    CHECK(textAt(frozen, "temperature_modifier") == "frozen");
    CHECK(plains.find("temperature_modifier") == nullptr);

    // An entry with no attributes at all writes no attributes key, rather than an empty one.
    const nbt::NbtCompound theEnd = vanillaBiomes::toNbt(find("the_end"));
    CHECK(theEnd.find("attributes") == nullptr);
    CHECK(nbt::snbt::stringify(nbt::NbtTag::ofCompound(theEnd)) ==
          R"({has_precipitation:0b,temperature:0.5f,downfall:0.5f,effects:{water_color:"#3f76e4"}})");
}

TEST_CASE("the values inside an attribute, not just its shape") {
    const auto find = [](std::string_view path) {
        for (const vanillaBiomes::Entry& entry : vanillaBiomes::all()) {
            if (entry.path == path) {
                return entry;
            }
        }
        FAIL("no such biome: ", path);
        return vanillaBiomes::Entry{};
    };

    // A music track's two delays, which are the same pair everywhere and so are easy to swap.
    const nbt::NbtCompound forest = vanillaBiomes::toNbt(find("forest"));
    const nbt::NbtCompound& plainsMusic = *forest.find("attributes")
                                               ->get<nbt::NbtCompound>()
                                               ->find("minecraft:audio/background_music")
                                               ->get<nbt::NbtCompound>();
    const nbt::NbtCompound& track = *plainsMusic.find("default")->get<nbt::NbtCompound>();
    CHECK(textAt(track, "sound") == "minecraft:music.overworld.forest");
    CHECK(track.getInt("min_delay") == 12000);
    CHECK(track.getInt("max_delay") == 24000);

    // The pale garden is the one biome with a plain number attribute, and with an empty music table
    // beside it: silence is a track list with nothing in it, not a missing key.
    const nbt::NbtCompound pale = vanillaBiomes::toNbt(find("pale_garden"));
    const nbt::NbtCompound& attributes = *pale.find("attributes")->get<nbt::NbtCompound>();
    CHECK(attributes.find("minecraft:audio/background_music")->get<nbt::NbtCompound>()->empty());
    CHECK(*attributes.find("minecraft:audio/music_volume")->get<float>() == 0.0F);

    // A mood carries its own delays rather than the shared defaults, even though every biome in the
    // table happens to use them.
    const vanillaBiomes::BiomeAttribute custom = vanillaBiomes::BiomeAttribute::sounds(
        "loop", vanillaBiomes::AmbientMood{"mood", 99, 7, 1.5}, vanillaBiomes::AmbientAddition{"add", 0.5});
    const vanillaBiomes::Entry made{.path = "made",
                                    .temperature = 1.0F,
                                    .downfall = 1.0F,
                                    .hasPrecipitation = false,
                                    .waterColor = "#000000",
                                    .attributes = {{"minecraft:audio/ambient_sounds", custom}}};
    // Named, not chained off the call: `toNbt` answers by value, and a reference through the
    // temporary would dangle the moment the expression ended.
    const nbt::NbtCompound written = vanillaBiomes::toNbt(made);
    const nbt::NbtCompound& mood = *written.find("attributes")
                                        ->get<nbt::NbtCompound>()
                                        ->find("minecraft:audio/ambient_sounds")
                                        ->get<nbt::NbtCompound>()
                                        ->find("mood")
                                        ->get<nbt::NbtCompound>();
    CHECK(mood.getInt("tick_delay") == 99);
    CHECK(mood.getInt("block_search_extent") == 7);
    CHECK(mood.getDouble("offset") == 1.5);
}

TEST_CASE("the nether biomes carry their sounds, music and particles in that order") {
    const auto find = [](std::string_view path) {
        for (const vanillaBiomes::Entry& entry : vanillaBiomes::netherBiomes()) {
            if (entry.path == path) {
                return entry;
            }
        }
        FAIL("no such biome: ", path);
        return vanillaBiomes::Entry{};
    };

    // With particles: sounds, music, particles, fog.
    const nbt::NbtCompound warped = vanillaBiomes::toNbt(find("warped_forest"));
    const nbt::NbtCompound& attributes = *warped.find("attributes")->get<nbt::NbtCompound>();
    REQUIRE(attributes.names().size() == 4);
    CHECK(attributes.names()[0] == "minecraft:audio/ambient_sounds");
    CHECK(attributes.names()[1] == "minecraft:audio/background_music");
    CHECK(attributes.names()[2] == "minecraft:visual/ambient_particles");
    CHECK(attributes.names()[3] == "minecraft:visual/fog_color");

    // Without: the particles are simply absent, and the rest keeps its place.
    const nbt::NbtCompound wastes = vanillaBiomes::toNbt(find("nether_wastes"));
    const nbt::NbtCompound& plain = *wastes.find("attributes")->get<nbt::NbtCompound>();
    REQUIRE(plain.names().size() == 3);
    CHECK(plain.names()[2] == "minecraft:visual/fog_color");

    // The mood and addition defaults are the ones every nether biome shares.
    const nbt::NbtCompound& sounds = *plain.find("minecraft:audio/ambient_sounds")->get<nbt::NbtCompound>();
    CHECK(textAt(sounds, "loop") == "minecraft:ambient.nether_wastes.loop");
    const nbt::NbtCompound& mood = *sounds.find("mood")->get<nbt::NbtCompound>();
    CHECK(textAt(mood, "sound") == "minecraft:ambient.nether_wastes.mood");
    CHECK(mood.getInt("tick_delay") == 6000);
    CHECK(mood.getInt("block_search_extent") == 8);
    CHECK(mood.getDouble("offset") == 2.0);
    const nbt::NbtCompound& additions = *sounds.find("additions")->get<nbt::NbtCompound>();
    CHECK(textAt(additions, "sound") == "minecraft:ambient.nether_wastes.additions");
    CHECK(additions.getDouble("tick_chance") == 0.0111);

    const nbt::NbtCompound& particle =
        *(*attributes.find("minecraft:visual/ambient_particles")->get<nbt::NbtList>())[0].get<nbt::NbtCompound>();
    CHECK(textAt(*particle.find("particle")->get<nbt::NbtCompound>(), "type") == "minecraft:warped_spore");
    CHECK(*particle.find("probability")->get<float>() == 0.01428F);

    // The nether is hot and dry, and none of it rains: the three fields every entry there shares.
    for (const vanillaBiomes::Entry& entry : vanillaBiomes::netherBiomes()) {
        CAPTURE(entry.path);
        CHECK(entry.temperature == 2.0F);
        CHECK(entry.downfall == 0.0F);
        CHECK_FALSE(entry.hasPrecipitation);
    }
    // The end is temperate on paper and equally dry.
    for (const vanillaBiomes::Entry& entry : vanillaBiomes::endBiomes()) {
        CAPTURE(entry.path);
        CHECK(entry.temperature == 0.5F);
        CHECK(entry.downfall == 0.5F);
        CHECK_FALSE(entry.hasPrecipitation);
        CHECK(entry.attributes.empty());
    }
}

TEST_CASE("the configuration packets go out in the order a client resolves ids in") {
    const std::vector<basalt::wire::protocol::configuration::RegistryDataPacket> packets =
        vanillaConfigurationRegistries::packets();
    REQUIRE(packets.size() == 21);

    std::vector<std::string> order;
    std::int32_t entries = 0;
    for (const auto& packet : packets) {
        order.push_back(packet.registryId);
        entries += static_cast<std::int32_t>(packet.entries.size());
    }
    std::string joined;
    for (const std::string& id : order) {
        if (!joined.empty()) {
            joined += ',';
        }
        joined += id;
    }
    CHECK(joined == "minecraft:world_clock,minecraft:dimension_type,minecraft:worldgen/biome,"
                    "minecraft:banner_pattern,minecraft:wolf_variant,minecraft:wolf_sound_variant,"
                    "minecraft:pig_variant,minecraft:pig_sound_variant,minecraft:frog_variant,"
                    "minecraft:cat_variant,minecraft:cat_sound_variant,minecraft:cow_sound_variant,"
                    "minecraft:cow_variant,minecraft:chicken_sound_variant,minecraft:chicken_variant,"
                    "minecraft:zombie_nautilus_variant,minecraft:painting_variant,minecraft:trim_material,"
                    "minecraft:damage_type,minecraft:jukebox_song,minecraft:instrument");
    // 2 clocks, 4 dimensions, 66 biomes, 43 banners, 51 across the animal variants, 1 painting,
    // 11 trims, 51 damage types, 22 songs and 8 instruments.
    CHECK(entries == 258);

    // Every key is namespaced, and every entry carries a value: an entry with none means "the
    // client already knows this one", which is not what any of these are.
    for (const auto& packet : packets) {
        CAPTURE(packet.registryId);
        CHECK_FALSE(packet.entries.empty());
        for (const auto& entry : packet.entries) {
            CHECK(entry.key.starts_with("minecraft:"));
            REQUIRE(entry.value.has_value());
            CHECK(entry.value->get<nbt::NbtCompound>() != nullptr);
        }
    }

    // The biome packet's own order is the palette order, so its index is the id a chunk references.
    const auto& biomes = packets[2];
    CHECK(biomes.entries.front().key == "minecraft:plains");
    CHECK(biomes.entries[29].key == "minecraft:meadow");
    CHECK(biomes.entries.back().key == "minecraft:end_barrens");

    // The painting registry has one entry, and it is named for the asset rather than the registry.
    CHECK(packets[16].entries.size() == 1);
    CHECK(packets[16].entries.front().key == "minecraft:humble");

    // Every entry's value is its own, not the first one's repeated: the wolf variants are the
    // clearest case, because each names its own asset.
    const auto& wolves = packets[4];
    REQUIRE(wolves.entries.size() == 9);
    CHECK(wolves.entries[0].key == "minecraft:pale");
    CHECK(wolves.entries[4].key == "minecraft:ashen");
    CHECK(textAt(*wolves.entries[4].value->get<nbt::NbtCompound>()->find("assets")->get<nbt::NbtCompound>(), "wild") ==
          "minecraft:entity/wolf/wolf_ashen");

    // And a biome's value is the biome, not an empty compound that happens to be one.
    CHECK(*biomes.entries.front().value->get<nbt::NbtCompound>()->find("temperature")->get<float>() == 0.8F);
    CHECK(biomes.entries.front().value->get<nbt::NbtCompound>()->find("effects") != nullptr);

    // A trim material's colour reaches its description rather than being dropped on the way.
    const auto& quartz = packets[17].entries.front();
    CHECK(quartz.key == "minecraft:quartz");
    CHECK(quartz.value->get<nbt::NbtCompound>()->find("description") != nullptr);
}
