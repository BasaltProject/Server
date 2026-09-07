#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/VillageDataLoader.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

namespace reg = server::registry;
namespace structure = server::world::structure;

using wire::common::Identifier;

/// parity: every name in these files may carry the `minecraft:` prefix or not, and both resolve to
/// the same file.
[[nodiscard]] std::string_view stripNamespace(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

} // namespace

VillageDataLoader::VillageDataLoader(StructureTemplates& templates, StructureTemplates::ReadEntry readEntry)
    : templates_(&templates), readEntry_(std::move(readEntry)) {}

std::optional<std::string> VillageDataLoader::readDataText(const std::string& path) const {
    if (!readEntry_) {
        return std::nullopt;
    }
    const std::optional<std::vector<std::uint8_t>> bytes = readEntry_("data/minecraft/" + path);
    return bytes.has_value() ? std::optional{std::string{bytes->begin(), bytes->end()}} : std::nullopt;
}

structure::TemplateLoader VillageDataLoader::templateLoader() {
    return [this](const Identifier& id) { return templates_->find(id); };
}

structure::villagePoolJson::TagResolver VillageDataLoader::tagResolver() {
    return [this](std::string_view name) { return blockTag(name); };
}

structure::villagePoolJson::ProcessorListResolver VillageDataLoader::processorResolver() {
    return [this](std::string_view name) { return processorListText(name); };
}

std::optional<std::string> VillageDataLoader::processorListText(std::string_view name) {
    {
        const std::scoped_lock lock{mutex_};
        if (const auto found = processorTexts_.find(name); found != processorTexts_.end()) {
            return found->second;
        }
    }
    const std::optional<std::string> text =
        readDataText("worldgen/processor_list/" + std::string{stripNamespace(name)} + ".json");
    const std::scoped_lock lock{mutex_};
    // Another thread may have answered the same name first; its entry is the one that stands.
    const auto [entry, inserted] = processorTexts_.emplace(std::string{name}, text);
    return entry->second;
}

const structure::StructureTemplatePool* VillageDataLoader::pool(std::string_view name) {
    {
        const std::scoped_lock lock{mutex_};
        if (const auto found = pools_.find(name); found != pools_.end()) {
            return found->second.has_value() ? &*found->second : nullptr;
        }
    }
    std::optional<structure::StructureTemplatePool> parsed;
    const std::optional<std::string> json =
        readDataText("worldgen/template_pool/" + std::string{stripNamespace(name)} + ".json");
    if (json.has_value()) {
        std::expected<structure::StructureTemplatePool, structure::villagePoolJson::ParseError> read =
            structure::villagePoolJson::parsePool(*json, processorResolver(), tagResolver());
        if (read.has_value()) {
            parsed = std::move(*read);
        }
    }
    const std::scoped_lock lock{mutex_};
    const auto [entry, inserted] = pools_.emplace(std::string{name}, std::move(parsed));
    return entry->second.has_value() ? &*entry->second : nullptr;
}

std::optional<std::set<std::int32_t>> VillageDataLoader::blockTag(std::string_view name) {
    {
        const std::scoped_lock lock{mutex_};
        if (const auto found = tags_.find(name); found != tags_.end()) {
            return found->second;
        }
    }
    std::set<std::string> seen;
    std::optional<std::set<std::int32_t>> expanded = expandBlockTag(name, seen);
    const std::scoped_lock lock{mutex_};
    const auto [entry, inserted] = tags_.emplace(std::string{name}, std::move(expanded));
    return entry->second;
}

std::optional<std::set<std::int32_t>> VillageDataLoader::expandBlockTag(std::string_view name,
                                                                        std::set<std::string>& seen) {
    // A tag that refers to itself, however far around, resolves to nothing rather than looping.
    if (!seen.insert(std::string{name}).second) {
        return std::set<std::int32_t>{};
    }
    const std::optional<std::string> json = readDataText("tags/block/" + std::string{stripNamespace(name)} + ".json");
    if (!json.has_value()) {
        return std::nullopt;
    }
    const std::expected<reg::tags::TagFile, reg::tags::tagFileJson::ParseError> file =
        reg::tags::tagFileJson::parse(*json);
    if (!file.has_value()) {
        return std::nullopt;
    }
    std::set<std::int32_t> states;
    for (const reg::tags::TagEntry& entry : file->entries) {
        if (entry.isTag) {
            if (const std::optional<std::set<std::int32_t>> nested = expandBlockTag(entry.id.toString(), seen);
                nested.has_value()) {
                states.insert(nested->begin(), nested->end());
            }
            continue;
        }
        if (const reg::BlockState* block = reg::blocks::named(entry.id.path()); block != nullptr) {
            states.insert(block->stateId);
        }
    }
    return states;
}

} // namespace basalt::server::runtime::world
