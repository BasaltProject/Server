#include <basalt/server/runtime/registry/VanillaBiomes.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaDimensionTypes.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>

#include <functional>
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

    // The one registry with a single entry, and its name is not the id it is looked up by.
    all.push_back(RegistryDataPacket{.registryId = "minecraft:painting_variant",
                                     .entries = {entryOf("humble", vanillaRegistryData::paintingVariant())}});

    RegistryDataPacket trims{.registryId = "minecraft:trim_material", .entries = {}};
    for (const auto& [id, color] : vanillaRegistryData::trimMaterials()) {
        trims.entries.push_back(entryOf(id, vanillaRegistryData::trimMaterial(id, color)));
    }
    all.push_back(std::move(trims));

    all.push_back(built("minecraft:damage_type", vanillaRegistryData::damageTypes()));
    all.push_back(built("minecraft:jukebox_song", vanillaRegistryData::jukeboxSongs()));
    all.push_back(built("minecraft:instrument", vanillaRegistryData::instruments()));
    return all;
}

} // namespace basalt::server::runtime::registry::vanillaConfigurationRegistries
