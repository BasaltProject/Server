#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/registry/VanillaBiomes.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaOverworldBiomes.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace basalt::server::runtime::registry;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] std::string textAt(const nbt::NbtCompound& value, std::string_view key) {
    const nbt::NbtTag* found = value.find(key);
    if (found == nullptr) {
        return "-";
    }
    const std::string* written = found->get<std::string>();
    return written == nullptr ? "-" : *written;
}

} // namespace

TEST_CASE("the synced biomes go out in the biome registry's own order") {
    // A chunk's biome palette names the client's ids, which it numbers in the order they arrive.
    const std::vector<vanillaBiomes::Entry>& all = vanillaBiomes::all();
    const auto& registry = basalt::server::registry::biomes::registry();
    REQUIRE(all.size() == registry.size());
    for (std::size_t id = 0; id < all.size(); ++id) {
        CAPTURE(id);
        CHECK(registry.idOf(basalt::wire::common::Identifier::vanilla(all[id].path)) == static_cast<std::int32_t>(id));
    }
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
    REQUIRE(packets.size() == 25);

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
                    "minecraft:chat_type,minecraft:banner_pattern,minecraft:wolf_variant,minecraft:wolf_sound_variant,"
                    "minecraft:pig_variant,minecraft:pig_sound_variant,minecraft:frog_variant,"
                    "minecraft:cat_variant,minecraft:cat_sound_variant,minecraft:cow_sound_variant,"
                    "minecraft:cow_variant,minecraft:chicken_sound_variant,minecraft:chicken_variant,"
                    "minecraft:zombie_nautilus_variant,minecraft:painting_variant,minecraft:trim_material,"
                    "minecraft:damage_type,minecraft:jukebox_song,minecraft:instrument,"
                    "minecraft:decorated_pot_pattern,minecraft:block_transformer,"
                    "minecraft:worldgen/block_state_provider");
    // 2 clocks, 4 dimensions, 67 biomes, 43 banners, 51 across the animal variants, 51 paintings,
    // 11 trims, 51 damage types, 22 songs, 8 instruments, 23 pot patterns, 3 transformers, 8
    // state providers and 7 chat types.
    CHECK(entries == 350);

    // Every key is namespaced. An entry with no value means "the client already knows this one",
    // which only the four registries 26.3 sends from the shared vanilla pack are.
    const std::set<std::string> known{"minecraft:chat_type", "minecraft:decorated_pot_pattern",
                                      "minecraft:block_transformer", "minecraft:worldgen/block_state_provider"};
    for (const auto& packet : packets) {
        CAPTURE(packet.registryId);
        CHECK_FALSE(packet.entries.empty());
        for (const auto& entry : packet.entries) {
            CHECK(entry.key.starts_with("minecraft:"));
            if (known.contains(packet.registryId)) {
                CHECK_FALSE(entry.value.has_value());
                continue;
            }
            REQUIRE(entry.value.has_value());
            CHECK(entry.value->get<nbt::NbtCompound>() != nullptr);
        }
    }

    // The biome packet's own order is the palette order, so its index is the id a chunk references.
    const auto& biomes = packets[2];
    CHECK(biomes.entries.front().key == "minecraft:plains");
    CHECK(biomes.entries[30].key == "minecraft:meadow");
    CHECK(biomes.entries.back().key == "minecraft:end_barrens");

    // Every painting the jar ships, each with its own size and its own two translation keys. A
    // client told about one of them draws every other as that one.
    const auto& paintings = packets[17];
    REQUIRE(paintings.entries.size() == 51);
    CHECK(paintings.entries.front().key == "minecraft:alban");
    CHECK(paintings.entries.back().key == "minecraft:wither");
    const nbt::NbtCompound& kebab = *paintings.entries[24].value->get<nbt::NbtCompound>();
    CHECK(*kebab.find("width")->get<std::int32_t>() == 1);
    CHECK(*kebab.find("height")->get<std::int32_t>() == 1);
    CHECK(textAt(kebab, "asset_id") == "minecraft:kebab");
    CHECK(kebab.find("author") != nullptr);
    // parity: the four elements and the wither are the five the jar signs with nobody.
    CHECK(paintings.entries.back().value->get<nbt::NbtCompound>()->find("author") == nullptr);

    // Every entry's value is its own, not the first one's repeated: the wolf variants are the
    // clearest case, because each names its own asset.
    const auto& wolves = packets[5];
    REQUIRE(wolves.entries.size() == 9);
    CHECK(wolves.entries[0].key == "minecraft:pale");
    CHECK(wolves.entries[4].key == "minecraft:ashen");
    CHECK(textAt(*wolves.entries[4].value->get<nbt::NbtCompound>()->find("assets")->get<nbt::NbtCompound>(), "wild") ==
          "minecraft:entity/wolf/wolf_ashen");

    // And a biome's value is the biome, not an empty compound that happens to be one.
    CHECK(*biomes.entries.front().value->get<nbt::NbtCompound>()->find("temperature")->get<float>() == 0.8F);
    CHECK(biomes.entries.front().value->get<nbt::NbtCompound>()->find("effects") != nullptr);

    // A trim material's colour reaches its description rather than being dropped on the way.
    const auto& quartz = packets[18].entries.front();
    CHECK(quartz.key == "minecraft:quartz");
    CHECK(quartz.value->get<nbt::NbtCompound>()->find("description") != nullptr);
}
