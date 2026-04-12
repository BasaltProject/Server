#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/server/runtime/registry/PackRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::registry {

void applyPackRegistries(std::vector<wire::protocol::configuration::RegistryDataPacket>& packets,
                         const datapack::ResourceStack& resources, const std::vector<std::string>& builtInPacks,
                         const wire::common::Logger& logger) {
    const datapack::EntriesUnder entriesUnder = [&resources](std::string_view prefix) {
        return resources.entriesUnder(prefix);
    };
    for (wire::protocol::configuration::RegistryDataPacket& packet : packets) {
        std::string_view directory = packet.registryId;
        if (directory.starts_with("minecraft:")) {
            directory.remove_prefix(std::string_view{"minecraft:"}.size());
        }
        for (const auto& [id, name] : datapack::dataFiles(entriesUnder, directory)) {
            const std::optional<std::string> pack = resources.packOf(name);
            if (!pack.has_value() || std::ranges::contains(builtInPacks, *pack)) {
                continue;
            }
            const std::optional<std::string> text = resources.readText(name);
            const auto json = text.has_value() ? wire::common::parseJson(*text)
                                               : std::expected<wire::common::JsonValue, wire::common::JsonError>{
                                                     std::unexpected{wire::common::JsonError::SyntaxError}};
            const auto tag = json.has_value() ? wire::nbt::jsonToNbt(*json)
                                              : std::expected<wire::nbt::NbtTag, wire::nbt::JsonToNbtError>{
                                                    std::unexpected{wire::nbt::JsonToNbtError::NullHasNoEquivalent}};
            if (!tag.has_value()) {
                logger.error([&] { return "Failed to parse " + id.toString() + " from pack " + *pack; });
                continue;
            }
            const std::string key = id.toString();
            const auto existing =
                std::ranges::find(packet.entries, key, &wire::protocol::configuration::RegistryEntry::key);
            if (existing != packet.entries.end()) {
                existing->value = *tag;
            } else {
                packet.entries.push_back(wire::protocol::configuration::RegistryEntry{.key = key, .value = *tag});
            }
        }
    }
}

namespace {

/// The registries whose entries come from data files and are numbered in load order.
constexpr std::array<std::string_view, 19> kSorted{"minecraft:banner_pattern",
                                                   "minecraft:wolf_variant",
                                                   "minecraft:wolf_sound_variant",
                                                   "minecraft:pig_variant",
                                                   "minecraft:pig_sound_variant",
                                                   "minecraft:frog_variant",
                                                   "minecraft:cat_variant",
                                                   "minecraft:cat_sound_variant",
                                                   "minecraft:cow_variant",
                                                   "minecraft:cow_sound_variant",
                                                   "minecraft:chicken_variant",
                                                   "minecraft:chicken_sound_variant",
                                                   "minecraft:zombie_nautilus_variant",
                                                   "minecraft:painting_variant",
                                                   "minecraft:trim_material",
                                                   "minecraft:damage_type",
                                                   "minecraft:jukebox_song",
                                                   "minecraft:instrument",
                                                   "minecraft:decorated_pot_pattern"};

[[nodiscard]] std::optional<std::vector<wire::protocol::configuration::RegistryDataPacket>>& installedState() {
    static std::optional<std::vector<wire::protocol::configuration::RegistryDataPacket>> state;
    return state;
}

} // namespace

void installRegistries(std::vector<wire::protocol::configuration::RegistryDataPacket> packets) {
    for (wire::protocol::configuration::RegistryDataPacket& packet : packets) {
        if (!std::ranges::contains(kSorted, packet.registryId)) {
            continue;
        }
        // parity: `ResourceManagerRegistryLoadTask` registers the elements sorted by key.
        std::ranges::stable_sort(packet.entries, [](const auto& left, const auto& right) {
            const auto a = wire::common::Identifier::parse(left.key);
            const auto b = wire::common::Identifier::parse(right.key);
            return a.has_value() && b.has_value() ? *a < *b : left.key < right.key;
        });
    }
    vanillaRegistryData::install(packets);
    installedState() = std::move(packets);
}

std::vector<wire::protocol::configuration::RegistryDataPacket> installedRegistries() {
    return installedState().has_value() ? *installedState() : vanillaConfigurationRegistries::packets();
}

} // namespace basalt::server::runtime::registry
