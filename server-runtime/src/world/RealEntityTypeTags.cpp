#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realEntityTypeTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

constexpr std::string_view kPrefix = "data/minecraft/tags/entity_type/";
constexpr std::string_view kJsonSuffix = ".json";

/// The tag's own name: its path under the tag directory, without the extension.
[[nodiscard]] Identifier nameOf(std::string_view entry) {
    if (entry.starts_with(kPrefix)) {
        entry.remove_prefix(kPrefix.size());
    }
    entry.remove_suffix(kJsonSuffix.size());
    return Identifier::vanilla(std::string{entry});
}

} // namespace

TagMembers load(const JarReader& jar, const wire::common::Logger& logger) {
    if (!jar.entriesUnder || !jar.readText) {
        return {};
    }
    std::map<Identifier, reg::tags::TagFile> files;
    for (const std::string& entry : jar.entriesUnder(kPrefix)) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        const std::optional<std::string> text = jar.readText(entry);
        if (!text.has_value()) {
            logger.debug([&] { return "Skipped entity type tag " + entry + ": unreadable"; });
            continue;
        }
        const std::expected<reg::tags::TagFile, reg::tags::tagFileJson::ParseError> file =
            reg::tags::tagFileJson::parse(*text);
        if (!file.has_value()) {
            logger.debug([&] { return "Skipped entity type tag " + entry + ": malformed"; });
            continue;
        }
        files.insert_or_assign(nameOf(entry), *file);
    }
    std::expected<TagMembers, reg::tags::TagError> resolved =
        reg::tags::resolve(files, [](const Identifier& id) { return reg::entityTypes::of(id) != nullptr; });
    if (!resolved.has_value()) {
        logger.warn([&] {
            return "Entity type tags could not be resolved, so `type=#tag` selectors match nobody: " +
                   resolved.error().message();
        });
        return {};
    }
    return std::move(*resolved);
}

const std::set<Identifier>& membersOf(const TagMembers& tags, const Identifier& tag) {
    static const std::set<Identifier> empty;
    const auto found = tags.find(tag);
    return found == tags.end() ? empty : found->second;
}

} // namespace basalt::server::runtime::world::realEntityTypeTags
