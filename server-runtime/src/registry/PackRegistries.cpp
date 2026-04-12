#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/server/runtime/registry/PackRegistries.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <algorithm>

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

} // namespace basalt::server::runtime::registry
