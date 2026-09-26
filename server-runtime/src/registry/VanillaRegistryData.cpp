#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <utility>

namespace basalt::server::runtime::registry::vanillaRegistryData {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

[[nodiscard]] NbtCompound wolfAssets(std::string_view wild, std::string_view tame, std::string_view angry) {
    NbtCompound value;
    value.putString("wild", std::string{wild});
    value.putString("tame", std::string{tame});
    value.putString("angry", std::string{angry});
    return value;
}

/// The sound names of one animal, which every variant of it derives from a prefix.
[[nodiscard]] NbtCompound soundsOf(std::string_view prefix, const std::vector<std::string_view>& kinds,
                                   std::string_view stepPrefix) {
    NbtCompound value;
    for (const std::string_view kind : kinds) {
        value.putString(std::string{kind} + "_sound",
                        "minecraft:entity." + std::string{prefix} + "." + std::string{kind});
    }
    value.putString("step_sound", "minecraft:entity." + std::string{stepPrefix} + ".step");
    return value;
}

[[nodiscard]] NbtCompound adultAndBaby(NbtCompound adult, NbtCompound baby) {
    NbtCompound value;
    value.set("adult_sounds", NbtTag::ofCompound(std::move(adult)));
    value.set("baby_sounds", NbtTag::ofCompound(std::move(baby)));
    return value;
}

[[nodiscard]] NbtCompound damageType(std::string_view messageId, float exhaustion,
                                     std::string_view scaling = "when_caused_by_living_non_player",
                                     std::string_view effects = {}, std::string_view deathMessageType = {}) {
    NbtCompound value;
    value.putString("message_id", std::string{messageId});
    value.putString("scaling", std::string{scaling});
    value.putFloat("exhaustion", exhaustion);
    if (!effects.empty()) {
        value.putString("effects", std::string{effects});
    }
    if (!deathMessageType.empty()) {
        value.putString("death_message_type", std::string{deathMessageType});
    }
    return value;
}

[[nodiscard]] NbtCompound jukeboxSong(std::string_view soundEvent, std::string_view translateKey, float lengthInSeconds,
                                      std::int32_t comparatorOutput) {
    NbtCompound value;
    value.putString("sound_event", std::string{soundEvent});
    value.set("description",
              wire::text::componentNbt::serialize(wire::text::Component::translatable(std::string{translateKey})));
    value.putFloat("length_in_seconds", lengthInSeconds);
    value.putInt("comparator_output", comparatorOutput);
    return value;
}

[[nodiscard]] NbtCompound instrumentOf(std::string_view soundEvent, std::string_view translateKey) {
    NbtCompound value;
    value.putString("sound_event", std::string{soundEvent});
    value.putFloat("use_duration", 7.0F);
    value.putFloat("range", 256.0F);
    value.set("description",
              wire::text::componentNbt::serialize(wire::text::Component::translatable(std::string{translateKey})));
    return value;
}

/// The synchronised registries as installed from the selected packs, or nothing before that.
struct Installed {
    std::map<std::string, std::vector<std::string>, std::less<>> paths;
    std::map<std::string, std::vector<Entry>, std::less<>> entries;
    std::optional<std::vector<Painting>> paintings;
    std::optional<std::vector<std::pair<std::string, std::int32_t>>> trims;
    /// Owns the ids the installed paintings view.
    std::deque<std::string> names;
};

[[nodiscard]] Installed& installed() {
    static Installed state;
    return state;
}

[[nodiscard]] const std::vector<std::string>& installedPaths(std::string_view registry,
                                                             const std::vector<std::string>& fallback) {
    const auto found = installed().paths.find(registry);
    return found == installed().paths.end() ? fallback : found->second;
}

[[nodiscard]] const std::vector<Entry>& installedEntries(std::string_view registry,
                                                         const std::vector<Entry>& fallback) {
    const auto found = installed().entries.find(registry);
    return found == installed().entries.end() ? fallback : found->second;
}

[[nodiscard]] const std::vector<Painting>& installedPaintings(const std::vector<Painting>& fallback) {
    return installed().paintings.has_value() ? *installed().paintings : fallback;
}

[[nodiscard]] const std::vector<std::pair<std::string, std::int32_t>>&
installedTrims(const std::vector<std::pair<std::string, std::int32_t>>& fallback) {
    return installed().trims.has_value() ? *installed().trims : fallback;
}

} // namespace

const std::vector<std::string>& bannerPatterns() {
    static const std::vector<std::string> built{"base",
                                                "square_bottom_left",
                                                "square_bottom_right",
                                                "square_top_left",
                                                "square_top_right",
                                                "stripe_bottom",
                                                "stripe_top",
                                                "stripe_left",
                                                "stripe_right",
                                                "stripe_center",
                                                "stripe_middle",
                                                "stripe_downright",
                                                "stripe_downleft",
                                                "small_stripes",
                                                "cross",
                                                "straight_cross",
                                                "triangle_bottom",
                                                "triangle_top",
                                                "triangles_bottom",
                                                "triangles_top",
                                                "diagonal_left",
                                                "diagonal_up_right",
                                                "diagonal_up_left",
                                                "diagonal_right",
                                                "circle",
                                                "rhombus",
                                                "half_vertical",
                                                "half_horizontal",
                                                "half_vertical_right",
                                                "half_horizontal_bottom",
                                                "border",
                                                "gradient",
                                                "gradient_up",
                                                "bricks",
                                                "curly_border",
                                                "globe",
                                                "creeper",
                                                "skull",
                                                "flower",
                                                "mojang",
                                                "piglin",
                                                "flow",
                                                "guster"};
    return installedPaths("minecraft:banner_pattern", built);
}

NbtCompound bannerPattern(std::string_view id) {
    NbtCompound value;
    value.putString("asset_id", "minecraft:" + std::string{id});
    value.putString("translation_key", "block.minecraft.banner." + std::string{id});
    return value;
}

const std::vector<std::string>& wolfVariants() {
    static const std::vector<std::string> built{"pale",  "spotted", "snowy",    "black",  "ashen",
                                                "rusty", "woods",   "chestnut", "striped"};
    return installedPaths("minecraft:wolf_variant", built);
}

NbtCompound wolfVariant(std::string_view id) {
    // The default variant's files are just `wolf`, not `wolf_pale`.
    const std::string fileName = id == "pale" ? "wolf" : "wolf_" + std::string{id};
    NbtCompound value;
    value.set("assets", NbtTag::ofCompound(wolfAssets("minecraft:entity/wolf/" + fileName,
                                                      "minecraft:entity/wolf/" + fileName + "_tame",
                                                      "minecraft:entity/wolf/" + fileName + "_angry")));
    value.set("baby_assets", NbtTag::ofCompound(wolfAssets("minecraft:entity/wolf/" + fileName + "_baby",
                                                           "minecraft:entity/wolf/" + fileName + "_tame_baby",
                                                           "minecraft:entity/wolf/" + fileName + "_angry_baby")));
    return value;
}

const std::vector<std::string>& wolfSoundVariants() {
    static const std::vector<std::string> built{"classic", "puglin", "sad", "angry", "grumpy", "big", "cute"};
    return installedPaths("minecraft:wolf_sound_variant", built);
}

NbtCompound wolfSoundVariant(std::string_view id) {
    static const std::vector<std::string_view> kKinds{"ambient", "death", "growl", "hurt", "pant", "whine"};
    const std::string adultPrefix = id == "classic" ? "wolf" : "wolf_" + std::string{id};
    // Every variant steps like a plain wolf, however it sounds otherwise.
    return adultAndBaby(soundsOf(adultPrefix, kKinds, "wolf"), soundsOf("baby_wolf", kKinds, "baby_wolf"));
}

const std::vector<std::string>& pigVariants() {
    static const std::vector<std::string> built{"temperate", "warm", "cold"};
    return installedPaths("minecraft:pig_variant", built);
}

NbtCompound pigVariant(std::string_view id) {
    NbtCompound value;
    value.putString("asset_id", "minecraft:entity/pig/pig_" + std::string{id});
    value.putString("baby_asset_id", "minecraft:entity/pig/pig_" + std::string{id} + "_baby");
    if (id == "cold") {
        value.putString("model", "cold");
    }
    return value;
}

const std::vector<std::string>& pigSoundVariants() {
    static const std::vector<std::string> built{"classic", "big", "mini"};
    return installedPaths("minecraft:pig_sound_variant", built);
}

NbtCompound pigSoundVariant(std::string_view id) {
    static const std::vector<std::string_view> kKinds{"ambient", "death", "eat", "hurt"};
    const std::string adultPrefix = id == "classic" ? "pig" : "pig_" + std::string{id};
    return adultAndBaby(soundsOf(adultPrefix, kKinds, "pig"), soundsOf("baby_pig", kKinds, "baby_pig"));
}

const std::vector<std::string>& frogVariants() {
    static const std::vector<std::string> built{"temperate", "warm", "cold"};
    return installedPaths("minecraft:frog_variant", built);
}

NbtCompound frogVariant(std::string_view id) {
    NbtCompound value;
    value.putString("asset_id", "minecraft:entity/frog/frog_" + std::string{id});
    return value;
}

const std::vector<std::string>& catVariants() {
    static const std::vector<std::string> built{"tabby",   "black",   "red",   "siamese", "british_shorthair", "calico",
                                                "persian", "ragdoll", "white", "jellie",  "all_black"};
    return installedPaths("minecraft:cat_variant", built);
}

NbtCompound catVariant(std::string_view id) {
    NbtCompound value;
    value.putString("asset_id", "minecraft:entity/cat/cat_" + std::string{id});
    value.putString("baby_asset_id", "minecraft:entity/cat/cat_" + std::string{id} + "_baby");
    return value;
}

const std::vector<std::string>& catSoundVariants() {
    static const std::vector<std::string> built{"classic", "royal"};
    return installedPaths("minecraft:cat_sound_variant", built);
}

NbtCompound catSoundVariant(std::string_view id) {
    static const std::vector<std::string_view> kKinds{"ambient", "beg_for_food", "death",   "eat",          "hiss",
                                                      "hurt",    "purr",         "purreow", "stray_ambient"};
    // The cat is the one animal whose sounds are not keyed on a `step`, so what would be the step
    // prefix is written as another named sound instead.
    NbtCompound adult;
    const std::string adultPrefix = id == "royal" ? "cat_royal" : "cat";
    for (const std::string_view kind : kKinds) {
        adult.putString(std::string{kind} + "_sound", "minecraft:entity." + adultPrefix + "." + std::string{kind});
    }
    NbtCompound baby;
    for (const std::string_view kind : kKinds) {
        baby.putString(std::string{kind} + "_sound", "minecraft:entity.baby_cat." + std::string{kind});
    }
    return adultAndBaby(std::move(adult), std::move(baby));
}

const std::vector<std::string>& cowVariants() {
    static const std::vector<std::string> built{"temperate", "warm", "cold"};
    return installedPaths("minecraft:cow_variant", built);
}

NbtCompound cowVariant(std::string_view id) {
    NbtCompound value;
    value.putString("asset_id", "minecraft:entity/cow/cow_" + std::string{id});
    value.putString("baby_asset_id", "minecraft:entity/cow/cow_" + std::string{id} + "_baby");
    if (id != "temperate") {
        value.putString("model", std::string{id});
    }
    return value;
}

const std::vector<std::string>& cowSoundVariants() {
    static const std::vector<std::string> built{"classic", "moody"};
    return installedPaths("minecraft:cow_sound_variant", built);
}

NbtCompound cowSoundVariant(std::string_view id) {
    // The cow is flat rather than split into adult and baby.
    const std::string prefix = id == "classic" ? "cow" : "cow_" + std::string{id};
    NbtCompound value;
    value.putString("ambient_sound", "minecraft:entity." + prefix + ".ambient");
    value.putString("death_sound", "minecraft:entity." + prefix + ".death");
    value.putString("hurt_sound", "minecraft:entity." + prefix + ".hurt");
    value.putString("step_sound", "minecraft:entity." + prefix + ".step");
    return value;
}

const std::vector<std::string>& chickenVariants() {
    static const std::vector<std::string> built{"temperate", "warm", "cold"};
    return installedPaths("minecraft:chicken_variant", built);
}

NbtCompound chickenVariant(std::string_view id) {
    NbtCompound value;
    value.putString("asset_id", "minecraft:entity/chicken/chicken_" + std::string{id});
    value.putString("baby_asset_id", "minecraft:entity/chicken/chicken_" + std::string{id} + "_baby");
    if (id == "cold") {
        value.putString("model", "cold");
    }
    return value;
}

const std::vector<std::string>& chickenSoundVariants() {
    static const std::vector<std::string> built{"classic", "picky"};
    return installedPaths("minecraft:chicken_sound_variant", built);
}

NbtCompound chickenSoundVariant(std::string_view id) {
    static const std::vector<std::string_view> kKinds{"ambient", "death", "hurt"};
    const std::string adultPrefix = id == "classic" ? "chicken" : "chicken_" + std::string{id};
    return adultAndBaby(soundsOf(adultPrefix, kKinds, "chicken"), soundsOf("baby_chicken", kKinds, "baby_chicken"));
}

const std::vector<std::string>& zombieNautilusVariants() {
    static const std::vector<std::string> built{"temperate", "warm"};
    return installedPaths("minecraft:zombie_nautilus_variant", built);
}

NbtCompound zombieNautilusVariant(std::string_view id) {
    NbtCompound value;
    value.putString("asset_id", id == "warm" ? "minecraft:entity/nautilus/zombie_nautilus_coral"
                                             : "minecraft:entity/nautilus/zombie_nautilus");
    if (id == "warm") {
        value.putString("model", "warm");
    }
    return value;
}

const std::vector<Painting>& paintingVariants() {
    // The jar's `painting_variant` folder, whose entries are all the same shape: the file name is
    // also the art and the stem of both translation keys.
    static const std::vector<Painting> built{
        {"alban", 1, 1},        {"aztec", 1, 1},       {"aztec2", 1, 1},       {"backyard", 3, 4},
        {"baroque", 2, 2},      {"bomb", 1, 1},        {"bouquet", 3, 3},      {"burning_skull", 4, 4},
        {"bust", 2, 2},         {"cavebird", 3, 3},    {"changing", 4, 2},     {"cotan", 3, 3},
        {"courbet", 2, 1},      {"creebet", 2, 1},     {"dennis", 3, 3},       {"donkey_kong", 4, 3},
        {"earth", 2, 2, false}, {"endboss", 3, 3},     {"fern", 3, 3},         {"fighters", 4, 2},
        {"finding", 4, 2},      {"fire", 2, 2, false}, {"graham", 1, 2},       {"humble", 2, 2},
        {"kebab", 1, 1},        {"lowmist", 4, 2},     {"match", 2, 2},        {"meditative", 1, 1},
        {"orb", 4, 4},          {"owlemons", 3, 3},    {"passage", 4, 2},      {"pigscene", 4, 4},
        {"plant", 1, 1},        {"pointer", 4, 4},     {"pond", 3, 4},         {"pool", 2, 1},
        {"prairie_ride", 1, 2}, {"sea", 2, 1},         {"skeleton", 4, 3},     {"skull_and_roses", 2, 2},
        {"stage", 2, 2},        {"sunflowers", 3, 3},  {"sunset", 2, 1},       {"tides", 3, 3},
        {"unpacked", 4, 4},     {"void", 2, 2},        {"wanderer", 1, 2},     {"wasteland", 1, 1},
        {"water", 2, 2, false}, {"wind", 2, 2, false}, {"wither", 2, 2, false}};
    return installedPaintings(built);
}

NbtCompound paintingVariant(const Painting& painting) {
    const std::string id{painting.id};
    NbtCompound value;
    value.putInt("width", painting.width);
    value.putInt("height", painting.height);
    value.putString("asset_id", "minecraft:" + id);
    // parity: every title is yellow and every author gray; the keys are the art's own name.
    value.set(
        "title",
        wire::text::componentNbt::serialize(
            wire::text::Component::translatable("painting.minecraft." + id + ".title").color(wire::text::kYellow)));
    if (painting.hasAuthor) {
        value.set(
            "author",
            wire::text::componentNbt::serialize(
                wire::text::Component::translatable("painting.minecraft." + id + ".author").color(wire::text::kGray)));
    }
    return value;
}

const std::vector<std::pair<std::string, std::int32_t>>& trimMaterials() {
    static const std::vector<std::pair<std::string, std::int32_t>> built{
        {"quartz", 14931140}, {"iron", 15527148},     {"netherite", 6445145}, {"redstone", 9901575},
        {"copper", 11823181}, {"gold", 14594349},     {"emerald", 1155126},   {"diamond", 7269586},
        {"lapis", 4288151},   {"amethyst", 10116294}, {"resin", 16545810}};
    return installedTrims(built);
}

NbtCompound trimMaterial(std::string_view id, std::int32_t colorRgb) {
    NbtCompound value;
    value.putString("palette_id", "minecraft:trim/" + std::string{id});
    value.set("description", wire::text::componentNbt::serialize(
                                 wire::text::Component::translatable("trim_material.minecraft." + std::string{id})
                                     .color(wire::text::TextColor{colorRgb})));
    return value;
}

const std::vector<Entry>& damageTypes() {
    static const std::vector<Entry> built{
        {"in_fire", damageType("inFire", 0.1F, "when_caused_by_living_non_player", "burning")},
        {"campfire", damageType("inFire", 0.1F, "when_caused_by_living_non_player", "burning")},
        {"lightning_bolt", damageType("lightningBolt", 0.1F)},
        {"on_fire", damageType("onFire", 0.0F, "when_caused_by_living_non_player", "burning")},
        {"lava", damageType("lava", 0.1F, "when_caused_by_living_non_player", "burning")},
        {"hot_floor", damageType("hotFloor", 0.1F, "when_caused_by_living_non_player", "burning")},
        {"sulfur_cube_hot", damageType("sulfurCubeHot", 0.1F, "when_caused_by_living_non_player", "burning")},
        {"in_wall", damageType("inWall", 0.0F)},
        {"cramming", damageType("cramming", 0.0F)},
        {"drown", damageType("drown", 0.0F, "when_caused_by_living_non_player", "drowning")},
        {"starve", damageType("starve", 0.0F)},
        {"cactus", damageType("cactus", 0.1F)},
        {"fall", damageType("fall", 0.0F, "when_caused_by_living_non_player", {}, "fall_variants")},
        {"ender_pearl", damageType("fall", 0.0F, "when_caused_by_living_non_player", {}, "fall_variants")},
        {"fly_into_wall", damageType("flyIntoWall", 0.0F)},
        {"out_of_world", damageType("outOfWorld", 0.0F)},
        {"generic", damageType("generic", 0.0F)},
        {"magic", damageType("magic", 0.0F)},
        {"wither", damageType("wither", 0.0F)},
        {"dragon_breath", damageType("dragonBreath", 0.0F)},
        {"dry_out", damageType("dryout", 0.1F)},
        {"sweet_berry_bush", damageType("sweetBerryBush", 0.1F, "when_caused_by_living_non_player", "poking")},
        {"freeze", damageType("freeze", 0.0F, "when_caused_by_living_non_player", "freezing")},
        {"stalagmite", damageType("stalagmite", 0.0F)},
        {"falling_block", damageType("fallingBlock", 0.1F)},
        {"falling_anvil", damageType("anvil", 0.1F)},
        {"falling_stalactite", damageType("fallingStalactite", 0.1F)},
        {"sting", damageType("sting", 0.1F)},
        {"mob_attack", damageType("mob", 0.1F)},
        {"mob_attack_no_aggro", damageType("mob", 0.1F)},
        {"player_attack", damageType("player", 0.1F)},
        {"spear", damageType("spear", 0.1F)},
        {"arrow", damageType("arrow", 0.1F)},
        {"trident", damageType("trident", 0.1F)},
        {"mob_projectile", damageType("mob", 0.1F)},
        {"spit", damageType("mob", 0.1F)},
        {"fireworks", damageType("fireworks", 0.1F)},
        {"unattributed_fireball", damageType("onFire", 0.1F, "when_caused_by_living_non_player", "burning")},
        {"fireball", damageType("fireball", 0.1F, "when_caused_by_living_non_player", "burning")},
        {"wither_skull", damageType("witherSkull", 0.1F)},
        {"thrown", damageType("thrown", 0.1F)},
        {"indirect_magic", damageType("indirectMagic", 0.0F)},
        {"thorns", damageType("thorns", 0.1F, "when_caused_by_living_non_player", "thorns")},
        {"explosion", damageType("explosion", 0.1F, "always")},
        {"player_explosion", damageType("explosion.player", 0.1F, "always")},
        {"sonic_boom", damageType("sonic_boom", 0.0F, "always")},
        {"bad_respawn_point", damageType("badRespawnPoint", 0.1F, "always", {}, "intentional_game_design")},
        {"outside_border", damageType("outsideBorder", 0.0F)},
        {"generic_kill", damageType("genericKill", 0.0F)},
        {"wind_charge", damageType("mob", 0.1F)},
        {"mace_smash", damageType("mace_smash", 0.1F)}};
    return installedEntries("minecraft:damage_type", built);
}

const std::vector<Entry>& jukeboxSongs() {
    static const std::vector<Entry> built{
        {"13", jukeboxSong("minecraft:music_disc.13", "jukebox_song.minecraft.13", 178.0F, 1)},
        {"cat", jukeboxSong("minecraft:music_disc.cat", "jukebox_song.minecraft.cat", 185.0F, 2)},
        {"blocks", jukeboxSong("minecraft:music_disc.blocks", "jukebox_song.minecraft.blocks", 345.0F, 3)},
        {"bounce", jukeboxSong("minecraft:music_disc.bounce", "jukebox_song.minecraft.bounce", 234.0F, 8)},
        {"chirp", jukeboxSong("minecraft:music_disc.chirp", "jukebox_song.minecraft.chirp", 185.0F, 4)},
        {"creator", jukeboxSong("minecraft:music_disc.creator", "jukebox_song.minecraft.creator", 176.0F, 12)},
        {"creator_music_box",
         jukeboxSong("minecraft:music_disc.creator_music_box", "jukebox_song.minecraft.creator_music_box", 73.0F, 11)},
        {"far", jukeboxSong("minecraft:music_disc.far", "jukebox_song.minecraft.far", 174.0F, 5)},
        {"lava_chicken",
         jukeboxSong("minecraft:music_disc.lava_chicken", "jukebox_song.minecraft.lava_chicken", 134.0F, 9)},
        {"mall", jukeboxSong("minecraft:music_disc.mall", "jukebox_song.minecraft.mall", 197.0F, 6)},
        {"mellohi", jukeboxSong("minecraft:music_disc.mellohi", "jukebox_song.minecraft.mellohi", 96.0F, 7)},
        {"stal", jukeboxSong("minecraft:music_disc.stal", "jukebox_song.minecraft.stal", 150.0F, 8)},
        {"strad", jukeboxSong("minecraft:music_disc.strad", "jukebox_song.minecraft.strad", 188.0F, 9)},
        {"ward", jukeboxSong("minecraft:music_disc.ward", "jukebox_song.minecraft.ward", 251.0F, 10)},
        {"11", jukeboxSong("minecraft:music_disc.11", "jukebox_song.minecraft.11", 71.0F, 11)},
        {"wait", jukeboxSong("minecraft:music_disc.wait", "jukebox_song.minecraft.wait", 238.0F, 12)},
        {"otherside", jukeboxSong("minecraft:music_disc.otherside", "jukebox_song.minecraft.otherside", 195.0F, 14)},
        {"relic", jukeboxSong("minecraft:music_disc.relic", "jukebox_song.minecraft.relic", 218.0F, 14)},
        {"5", jukeboxSong("minecraft:music_disc.5", "jukebox_song.minecraft.5", 178.0F, 15)},
        {"pigstep", jukeboxSong("minecraft:music_disc.pigstep", "jukebox_song.minecraft.pigstep", 149.0F, 13)},
        {"precipice", jukeboxSong("minecraft:music_disc.precipice", "jukebox_song.minecraft.precipice", 299.0F, 13)},
        {"tears", jukeboxSong("minecraft:music_disc.tears", "jukebox_song.minecraft.tears", 175.0F, 10)}};
    return installedEntries("minecraft:jukebox_song", built);
}

const std::vector<Entry>& instruments() {
    static const std::vector<Entry> built{
        {"ponder_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.0", "instrument.minecraft.ponder_goat_horn")},
        {"sing_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.1", "instrument.minecraft.sing_goat_horn")},
        {"seek_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.2", "instrument.minecraft.seek_goat_horn")},
        {"feel_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.3", "instrument.minecraft.feel_goat_horn")},
        {"admire_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.4", "instrument.minecraft.admire_goat_horn")},
        {"call_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.5", "instrument.minecraft.call_goat_horn")},
        {"yearn_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.6", "instrument.minecraft.yearn_goat_horn")},
        {"dream_goat_horn", instrumentOf("minecraft:item.goat_horn.sound.7", "instrument.minecraft.dream_goat_horn")}};
    return installedEntries("minecraft:instrument", built);
}

void install(const std::vector<wire::protocol::configuration::RegistryDataPacket>& packets) {
    // Built before anything reads it, so the paths the defaults returned are the same objects.
    const std::vector<Painting> vanillaPaintings = paintingVariants();
    const std::vector<std::pair<std::string, std::int32_t>> vanillaTrims = trimMaterials();
    Installed& state = installed();
    for (const wire::protocol::configuration::RegistryDataPacket& packet : packets) {
        std::vector<std::string> keys;
        std::vector<Entry> entries;
        for (const wire::protocol::configuration::RegistryEntry& entry : packet.entries) {
            std::string_view key = entry.key;
            if (key.starts_with("minecraft:")) {
                key.remove_prefix(std::string_view{"minecraft:"}.size());
            }
            keys.emplace_back(key);
            const auto* compound = entry.value.has_value() ? entry.value->get<NbtCompound>() : nullptr;
            entries.emplace_back(std::string{key}, compound == nullptr ? NbtCompound{} : *compound);
        }
        const std::string& registry = packet.registryId;
        if (registry == "minecraft:painting_variant") {
            std::vector<Painting> paintings;
            for (const Entry& entry : entries) {
                const auto known = std::ranges::find(vanillaPaintings, entry.first, &Painting::id);
                if (known != vanillaPaintings.end()) {
                    paintings.push_back(*known);
                    continue;
                }
                state.names.push_back(entry.first);
                paintings.push_back(Painting{.id = state.names.back(),
                                             .width = entry.second.getInt("width", 1),
                                             .height = entry.second.getInt("height", 1),
                                             .hasAuthor = entry.second.contains("author")});
            }
            state.paintings = std::move(paintings);
        } else if (registry == "minecraft:trim_material") {
            std::vector<std::pair<std::string, std::int32_t>> trims;
            for (const std::string& key : keys) {
                const auto known = std::ranges::find(vanillaTrims, key, &std::pair<std::string, std::int32_t>::first);
                trims.emplace_back(key, known == vanillaTrims.end() ? 0 : known->second);
            }
            state.trims = std::move(trims);
        } else if (registry == "minecraft:damage_type" || registry == "minecraft:jukebox_song" ||
                   registry == "minecraft:instrument") {
            state.entries.insert_or_assign(registry, std::move(entries));
        } else {
            state.paths.insert_or_assign(registry, std::move(keys));
        }
    }
}

} // namespace basalt::server::runtime::registry::vanillaRegistryData
