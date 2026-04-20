#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::datapack {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

constexpr std::string_view kDataRoot = "data/";
constexpr std::string_view kTagsDirectory = "tags/";
constexpr std::string_view kJsonSuffix = ".json";

[[nodiscard]] std::optional<std::pair<std::string_view, std::string_view>> split(std::string_view name) {
    if (!name.starts_with(kDataRoot)) {
        return std::nullopt;
    }
    name.remove_prefix(kDataRoot.size());
    const std::size_t slash = name.find('/');
    if (slash == std::string_view::npos) {
        return std::nullopt;
    }
    return std::pair{name.substr(0, slash), name.substr(slash + 1)};
}

[[nodiscard]] bool isTag(std::string_view name) {
    const auto parts = split(name);
    return parts.has_value() && parts->second.starts_with(kTagsDirectory) && name.ends_with(kJsonSuffix);
}

[[nodiscard]] std::string tagIdOf(std::string_view name) {
    const auto parts = split(name);
    std::string_view path = parts->second.substr(kTagsDirectory.size());
    path.remove_suffix(kJsonSuffix.size());
    return std::string{parts->first} + ":" + std::string{path};
}

[[nodiscard]] JsonValue entryJson(const registry::tags::TagEntry& entry) {
    const std::string id = (entry.isTag ? "#" : "") + entry.id.toString();
    if (entry.required) {
        return JsonValue::of(id);
    }
    JsonObject object;
    object.set("id", JsonValue::of(id));
    object.set("required", JsonValue::of(false));
    return JsonValue::ofObject(std::move(object));
}

} // namespace

ResourceStack::ResourceStack(std::vector<Layer> layers, wire::common::Logger logger)
    : layers_(std::move(layers)), logger_(std::move(logger)) {
    for (const Layer& layer : layers_) {
        std::vector<CompiledPattern> compiled;
        for (const IdentifierPattern& pattern : layer.filter) {
            CompiledPattern one;
            if (pattern.namespacePattern.has_value()) {
                one.nameSpace.emplace(*pattern.namespacePattern);
            }
            if (pattern.pathPattern.has_value()) {
                one.path.emplace(*pattern.pathPattern);
            }
            compiled.push_back(std::move(one));
        }
        compiled_.push_back(std::move(compiled));
    }
    for (std::size_t index = 0; index < layers_.size(); ++index) {
        if (!compiled_[index].empty()) {
            std::erase_if(visible_, [this, index](const auto& entry) { return filters(index, entry.first); });
        }
        if (layers_[index].contents == nullptr) {
            continue;
        }
        for (const auto& [name, unused] : layers_[index].contents->index()) {
            visible_.insert_or_assign(name, index);
        }
    }
}

bool ResourceStack::filters(std::size_t layer, std::string_view name) const {
    const std::vector<CompiledPattern>& patterns = compiled_[layer];
    if (patterns.empty()) {
        return false;
    }
    const auto parts = split(name);
    if (!parts.has_value()) {
        return false;
    }
    const std::string nameSpace{parts->first};
    const std::string path{parts->second};
    const bool namespaceMatches = std::ranges::any_of(patterns, [&nameSpace](const CompiledPattern& pattern) {
        return !pattern.nameSpace.has_value() || std::regex_search(nameSpace, *pattern.nameSpace);
    });
    return namespaceMatches && std::ranges::any_of(patterns, [&path](const CompiledPattern& pattern) {
               return !pattern.path.has_value() || std::regex_search(path, *pattern.path);
           });
}

std::optional<std::vector<std::uint8_t>> ResourceStack::read(std::string_view name) const {
    const auto found = visible_.find(name);
    if (found == visible_.end()) {
        return std::nullopt;
    }
    return layers_[found->second].contents->read(found->first);
}

std::optional<std::string> ResourceStack::readText(std::string_view name) const {
    if (isTag(name)) {
        return mergedTag(name);
    }
    std::optional<std::vector<std::uint8_t>> bytes = read(name);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    return std::string{bytes->begin(), bytes->end()};
}

std::vector<std::string> ResourceStack::entriesUnder(std::string_view prefix) const {
    std::vector<std::string> names;
    for (auto at = visible_.lower_bound(prefix); at != visible_.end() && at->first.starts_with(prefix); ++at) {
        names.push_back(at->first);
    }
    return names;
}

std::vector<ResourceStack::Source> ResourceStack::stackOf(std::string_view name) const {
    std::vector<Source> sources;
    const std::string key{name};
    for (std::size_t index = 0; index < layers_.size(); ++index) {
        if (filters(index, name)) {
            sources.clear();
        }
        const PackContents* contents = layers_[index].contents.get();
        if (contents == nullptr || !contents->contains(key)) {
            continue;
        }
        std::optional<std::vector<std::uint8_t>> bytes = contents->read(key);
        if (bytes.has_value()) {
            sources.push_back(Source{.packId = contents->packId(), .bytes = std::move(*bytes)});
        }
    }
    return sources;
}

std::optional<std::string> ResourceStack::mergedTag(std::string_view name) const {
    const std::vector<Source> sources = stackOf(name);
    if (sources.empty()) {
        return std::nullopt;
    }
    std::vector<JsonValue> values;
    bool any = false;
    for (const Source& source : sources) {
        const std::string text{source.bytes.begin(), source.bytes.end()};
        const std::expected<registry::tags::TagFile, registry::tags::tagFileJson::ParseError> file =
            registry::tags::tagFileJson::parse(text);
        if (!file.has_value()) {
            logger_.error([&] {
                const auto parts = split(name);
                return "Couldn't read tag list " + tagIdOf(name) + " from " + std::string{parts->first} + ":" +
                       std::string{parts->second} + " in data pack " + source.packId;
            });
            continue;
        }
        any = true;
        if (file->replace) {
            values.clear();
        }
        for (const registry::tags::TagEntry& entry : file->entries) {
            values.push_back(entryJson(entry));
        }
    }
    if (!any) {
        return std::nullopt;
    }
    JsonObject root;
    root.set("values", JsonValue::ofArray(std::move(values)));
    return wire::common::writeJson(JsonValue::ofObject(std::move(root)));
}

std::vector<Identifier> ResourceStack::dataIds(std::string_view directory, std::string_view extension) const {
    std::vector<Identifier> ids;
    for (const std::string& name : entriesUnder(kDataRoot)) {
        const auto parts = split(name);
        if (!parts.has_value()) {
            continue;
        }
        const std::string wanted = std::string{directory} + "/";
        if (!parts->second.starts_with(wanted) || !parts->second.ends_with(extension)) {
            continue;
        }
        const std::string_view path =
            parts->second.substr(wanted.size(), parts->second.size() - wanted.size() - extension.size());
        if (!path.empty()) {
            ids.emplace_back(std::string{parts->first}, std::string{path});
        }
    }
    return ids;
}

std::optional<std::string> ResourceStack::packOf(std::string_view name) const {
    const auto found = visible_.find(name);
    if (found == visible_.end()) {
        return std::nullopt;
    }
    return layers_[found->second].contents->packId();
}

std::vector<std::string> ResourceStack::packIds() const {
    std::vector<std::string> ids;
    for (const Layer& layer : layers_) {
        if (layer.contents != nullptr) {
            ids.push_back(layer.contents->packId());
        }
    }
    return ids;
}

} // namespace basalt::server::runtime::datapack
