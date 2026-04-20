#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/BiomeTagLoader.hpp>

namespace basalt::server::runtime::world {
namespace {

/// The namespace of a tag name is the jar's own, so only the path after it names the file.
[[nodiscard]] std::string_view pathOf(std::string_view name) {
    const std::size_t colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

} // namespace

const std::set<std::string>& BiomeTagLoader::biomesIn(const std::string& name) {
    const auto cached = cache_.find(name);
    if (cached != cache_.end()) {
        return cached->second;
    }
    std::set<std::string> seen;
    return cache_.emplace(name, expand(name, seen)).first->second;
}

std::set<std::string> BiomeTagLoader::expand(const std::string& name, std::set<std::string>& seen) const {
    if (!seen.insert(name).second) {
        return {};
    }
    const std::string entry = "data/minecraft/tags/worldgen/biome/" + std::string{pathOf(name)} + ".json";
    const std::optional<std::string> json = readEntry_ ? readEntry_(entry) : std::nullopt;
    if (!json.has_value()) {
        return {};
    }
    const std::expected<registry::tags::TagFile, registry::tags::tagFileJson::ParseError> file =
        registry::tags::tagFileJson::parse(*json);
    if (!file.has_value()) {
        return {};
    }
    std::set<std::string> result;
    for (const registry::tags::TagEntry& value : file->entries) {
        if (value.isTag) {
            result.merge(expand(value.id.toString(), seen));
            continue;
        }
        result.insert(value.id.toString());
    }
    return result;
}

} // namespace basalt::server::runtime::world
