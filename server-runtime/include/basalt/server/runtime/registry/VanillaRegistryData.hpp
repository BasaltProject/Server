#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

namespace vanillaRegistryData {

using Entry = std::pair<std::string, wire::nbt::NbtCompound>;

[[nodiscard]] const std::vector<std::string>& bannerPatterns();
[[nodiscard]] wire::nbt::NbtCompound bannerPattern(std::string_view id);

/// Vanilla bootstrap order; these indices are also sent by wolf entity metadata.
[[nodiscard]] const std::vector<std::string>& wolfVariants();
[[nodiscard]] wire::nbt::NbtCompound wolfVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& wolfSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound wolfSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& pigVariants();
[[nodiscard]] wire::nbt::NbtCompound pigVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& pigSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound pigSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& frogVariants();
[[nodiscard]] wire::nbt::NbtCompound frogVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& catVariants();
[[nodiscard]] wire::nbt::NbtCompound catVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& catSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound catSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& cowVariants();
[[nodiscard]] wire::nbt::NbtCompound cowVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& cowSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound cowSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& chickenVariants();
[[nodiscard]] wire::nbt::NbtCompound chickenVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& chickenSoundVariants();
[[nodiscard]] wire::nbt::NbtCompound chickenSoundVariant(std::string_view id);

[[nodiscard]] const std::vector<std::string>& zombieNautilusVariants();
[[nodiscard]] wire::nbt::NbtCompound zombieNautilusVariant(std::string_view id);

struct Painting {
    std::string_view id;
    std::int32_t width = 1;
    std::int32_t height = 1;
    bool hasAuthor = true;
};

[[nodiscard]] const std::vector<Painting>& paintingVariants();

[[nodiscard]] wire::nbt::NbtCompound paintingVariant(const Painting& painting);

[[nodiscard]] const std::vector<std::pair<std::string, std::int32_t>>& trimMaterials();
[[nodiscard]] wire::nbt::NbtCompound trimMaterial(std::string_view id, std::int32_t colorRgb);

[[nodiscard]] const std::vector<Entry>& damageTypes();

[[nodiscard]] const std::vector<Entry>& jukeboxSongs();

[[nodiscard]] const std::vector<Entry>& instruments();

void install(const std::vector<wire::protocol::configuration::RegistryDataPacket>& packets);

} // namespace vanillaRegistryData

} // namespace basalt::server::runtime::registry
