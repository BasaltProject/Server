#include <basalt/server/runtime/registry/VanillaBiomes.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaDimensionTypes.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>

#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::registry::vanillaConfigurationRegistries {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::protocol::configuration::RegistryDataPacket;
using wire::protocol::configuration::RegistryEntry;

[[nodiscard]] RegistryEntry entryOf(std::string_view path, NbtCompound value) {
    return RegistryEntry{.key = "minecraft:" + std::string{path}, .value = NbtTag::ofCompound(std::move(value))};
}

/// One registry whose entries are built from their own names, which is most of the variant ones.
[[nodiscard]] RegistryDataPacket variants(std::string registryId, const std::vector<std::string>& paths,
                                          const std::function<NbtCompound(std::string_view)>& value) {
    RegistryDataPacket packet{.registryId = std::move(registryId), .entries = {}};
    for (const std::string& path : paths) {
        packet.entries.push_back(entryOf(path, value(path)));
    }
    return packet;
}

/// One registry whose entries are already built, which is the rest.
[[nodiscard]] RegistryDataPacket built(std::string registryId, const std::vector<vanillaRegistryData::Entry>& entries) {
    RegistryDataPacket packet{.registryId = std::move(registryId), .entries = {}};
    for (const auto& [path, value] : entries) {
        packet.entries.push_back(entryOf(path, value));
    }
    return packet;
}

/// A registry whose entries the client reads from its own copy of the vanilla pack.
///
/// parity: `RegistrySynchronization.packRegistry` leaves out the data of an entry from a pack both
/// ends know, and these four are only ever the vanilla ones. Sorted, as data pack loading is.
[[nodiscard]] RegistryDataPacket known(std::string registryId, std::initializer_list<std::string_view> paths) {
    RegistryDataPacket packet{.registryId = std::move(registryId), .entries = {}};
    for (const std::string_view path : paths) {
        packet.entries.push_back(RegistryEntry{.key = "minecraft:" + std::string{path}, .value = std::nullopt});
    }
    return packet;
}

} // namespace

std::vector<RegistryDataPacket> packets() {
    std::vector<RegistryDataPacket> all;

    all.push_back(built("minecraft:world_clock", vanillaDimensionTypes::worldClocks()));
    all.push_back(built("minecraft:dimension_type", vanillaDimensionTypes::dimensionTypes()));

    RegistryDataPacket biomes{.registryId = "minecraft:worldgen/biome", .entries = {}};
    for (const vanillaBiomes::Entry& entry : vanillaBiomes::all()) {
        biomes.entries.push_back(entryOf(entry.path, vanillaBiomes::toNbt(entry)));
    }
    all.push_back(std::move(biomes));
    // parity: right after the biomes, as `SYNCHRONIZED_REGISTRIES` lists it. Their order is what a
    // chat packet's chat type number refers to.
    all.push_back(
        known("minecraft:chat_type", {"chat", "emote_command", "msg_command_incoming", "msg_command_outgoing",
                                      "say_command", "team_msg_command_incoming", "team_msg_command_outgoing"}));

    all.push_back(variants("minecraft:banner_pattern", vanillaRegistryData::bannerPatterns(),
                           vanillaRegistryData::bannerPattern));
    all.push_back(
        variants("minecraft:wolf_variant", vanillaRegistryData::wolfVariants(), vanillaRegistryData::wolfVariant));
    all.push_back(variants("minecraft:wolf_sound_variant", vanillaRegistryData::wolfSoundVariants(),
                           vanillaRegistryData::wolfSoundVariant));
    all.push_back(
        variants("minecraft:pig_variant", vanillaRegistryData::pigVariants(), vanillaRegistryData::pigVariant));
    all.push_back(variants("minecraft:pig_sound_variant", vanillaRegistryData::pigSoundVariants(),
                           vanillaRegistryData::pigSoundVariant));
    all.push_back(
        variants("minecraft:frog_variant", vanillaRegistryData::frogVariants(), vanillaRegistryData::frogVariant));
    all.push_back(
        variants("minecraft:cat_variant", vanillaRegistryData::catVariants(), vanillaRegistryData::catVariant));
    all.push_back(variants("minecraft:cat_sound_variant", vanillaRegistryData::catSoundVariants(),
                           vanillaRegistryData::catSoundVariant));
    all.push_back(variants("minecraft:cow_sound_variant", vanillaRegistryData::cowSoundVariants(),
                           vanillaRegistryData::cowSoundVariant));
    all.push_back(
        variants("minecraft:cow_variant", vanillaRegistryData::cowVariants(), vanillaRegistryData::cowVariant));
    all.push_back(variants("minecraft:chicken_sound_variant", vanillaRegistryData::chickenSoundVariants(),
                           vanillaRegistryData::chickenSoundVariant));
    all.push_back(variants("minecraft:chicken_variant", vanillaRegistryData::chickenVariants(),
                           vanillaRegistryData::chickenVariant));
    all.push_back(variants("minecraft:zombie_nautilus_variant", vanillaRegistryData::zombieNautilusVariants(),
                           vanillaRegistryData::zombieNautilusVariant));

    RegistryDataPacket paintings{.registryId = "minecraft:painting_variant", .entries = {}};
    for (const vanillaRegistryData::Painting& painting : vanillaRegistryData::paintingVariants()) {
        paintings.entries.push_back(entryOf(painting.id, vanillaRegistryData::paintingVariant(painting)));
    }
    all.push_back(std::move(paintings));

    RegistryDataPacket trims{.registryId = "minecraft:trim_material", .entries = {}};
    for (const auto& [id, color] : vanillaRegistryData::trimMaterials()) {
        trims.entries.push_back(entryOf(id, vanillaRegistryData::trimMaterial(id, color)));
    }
    all.push_back(std::move(trims));

    all.push_back(built("minecraft:damage_type", vanillaRegistryData::damageTypes()));
    all.push_back(built("minecraft:jukebox_song", vanillaRegistryData::jukeboxSongs()));
    all.push_back(built("minecraft:instrument", vanillaRegistryData::instruments()));
    all.push_back(known("minecraft:decorated_pot_pattern",
                        {"angler", "archer", "arms_up", "blade", "brewer",     "burn",  "danger", "explorer",
                         "flow",   "friend", "guster",  "heart", "heartbreak", "howl",  "miner",  "mourner",
                         "plenty", "prize",  "scrape",  "sheaf", "shelter",    "skull", "snort"}));
    all.push_back(known("minecraft:block_transformer", {"axe", "hoe", "shovel"}));

    all.push_back(known("minecraft:worldgen/block_state_provider",
                        {"cave_vines_body", "cave_vines_head", "flower_flower_forest", "flower_meadow", "flower_plain",
                         "mangrove_propagule", "podzol_beneath_tree", "soil_beneath_tree"}));
    return all;
}

} // namespace basalt::server::runtime::registry::vanillaConfigurationRegistries
