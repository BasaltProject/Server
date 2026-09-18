#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

/// Hand-authored NBT for the static registries a real 26.2 client needs during Configuration: the
/// ones it hard-requires non-empty, plus the variant and banner registries entity metadata refers to.
///
/// Field shapes come from decompiling the real 26.2 jar's codecs, not from memory: the schemas
/// changed substantially from older versions. The full synced set is a gap `server-datagen` fills.
namespace vanillaRegistryData {

using Entry = std::pair<std::string, wire::nbt::NbtCompound>;

/// parity: `BannerPatterns.bootstrap`, including the holder ids banner item components use.
[[nodiscard]] const std::vector<std::string>& bannerPatterns();
[[nodiscard]] wire::nbt::NbtCompound bannerPattern(std::string_view id);

/// Vanilla bootstrap order; these indices are also sent by wolf entity metadata.
[[nodiscard]] const std::vector<std::string>& wolfVariants();
[[nodiscard]] wire::nbt::NbtCompound wolfVariant(std::string_view id);

/// Vanilla bootstrap order; every natural wolf picks one independently.
[[nodiscard]] const std::vector<std::string>& wolfSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound wolfSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& pigVariants();
[[nodiscard]] wire::nbt::NbtCompound pigVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& pigSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound pigSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& frogVariants();
[[nodiscard]] wire::nbt::NbtCompound frogVariant(std::string_view id);

/// All eleven variants, in `CatVariants.bootstrap` order. The full set matters, not just one entry:
/// structure cats carry a variant id into `set_entity_data`, and a client whose registry holds fewer
/// entries than the id being sent drops the connection failing to decode it.
[[nodiscard]] const std::vector<std::string>& catVariants();
[[nodiscard]] wire::nbt::NbtCompound catVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& catSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound catSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& cowVariants();
[[nodiscard]] wire::nbt::NbtCompound cowVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& cowSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound cowSoundVariant(std::string_view id);

/// Three entries, not just the default: the egg, blue_egg and brown_egg items each carry a default
/// component resolving a distinct key (temperate, cold, warm), confirmed live.
[[nodiscard]] const std::vector<std::string>& chickenVariants();
[[nodiscard]] wire::nbt::NbtCompound chickenVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& chickenSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound chickenSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& zombieNautilusVariants();
[[nodiscard]] wire::nbt::NbtCompound zombieNautilusVariant(std::string_view id);

/// One painting as the client draws it: the id names both the art and its two translation keys.
struct Painting {
    std::string_view id;
    std::int32_t width = 1;
    std::int32_t height = 1;
    /// The four elements and the wither are signed by nobody, so their entry has no author.
    bool hasAuthor = true;
};

[[nodiscard]] const std::vector<Painting>& paintingVariants();

[[nodiscard]] wire::nbt::NbtCompound paintingVariant(const Painting& painting);

/// Not in the must-be-non-empty set, but several base items carry a default `ProvidesTrimMaterial`
/// component resolving a specific entry by key, so all eleven items reference are needed. Field
/// shapes from `TrimMaterial.DIRECT_CODEC`, which is what syncs it: there is no `NETWORK_CODEC`.
[[nodiscard]] const std::vector<std::pair<std::string, std::int32_t>>& trimMaterials();
[[nodiscard]] wire::nbt::NbtCompound trimMaterial(std::string_view id, std::int32_t colorRgb);

/// Not in the set either, but a trident carries a default damage type component resolving
/// `minecraft:damage_type/minecraft:spear` by key: a real client failed there right after
/// `trim_material` was fixed. The whole registry goes out rather than just `spear`, since every
/// entry is equally cheap and death-message text will want them.
[[nodiscard]] const std::vector<Entry>& damageTypes();

/// Every music disc carries a default component resolving its song by key. Values verbatim from the
/// jar's embedded `data/minecraft/jukebox_song/*.json`.
[[nodiscard]] const std::vector<Entry>& jukeboxSongs();

/// The goat horn carries a default instrument component resolving `ponder_goat_horn` by key. All
/// eight are included; they differ only by the sound event's index.
[[nodiscard]] const std::vector<Entry>& instruments();

} // namespace vanillaRegistryData

} // namespace basalt::server::runtime::registry
