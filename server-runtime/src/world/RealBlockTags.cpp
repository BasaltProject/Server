#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realBlockTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

constexpr std::string_view kPrefix = "data/minecraft/tags/block/";
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
            logger.debug([&] { return "Skipped block tag " + entry + ": unreadable"; });
            continue;
        }
        const std::expected<reg::tags::TagFile, reg::tags::tagFileJson::ParseError> file =
            reg::tags::tagFileJson::parse(*text);
        if (!file.has_value()) {
            logger.debug([&] { return "Skipped block tag " + entry + ": malformed"; });
            continue;
        }
        files.insert_or_assign(nameOf(entry), *file);
    }
    std::expected<TagMembers, reg::tags::TagError> resolved =
        reg::tags::resolve(files, [](const Identifier& id) { return reg::blocks::registry().byName(id) != nullptr; });
    if (!resolved.has_value()) {
        logger.warn([&] {
            return "Block tags could not be resolved, so tool requirements will not apply: " +
                   resolved.error().message();
        });
        return {};
    }
    return std::move(*resolved);
}

const std::set<Identifier>& membersOf(const TagMembers& tags, std::string_view tag) {
    static const std::set<Identifier> empty;
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(tag);
    if (!id.has_value()) {
        return empty;
    }
    const auto found = tags.find(*id);
    return found == tags.end() ? empty : found->second;
}

bool contains(const TagMembers& tags, std::string_view tag, const Identifier& block) {
    return membersOf(tags, tag).contains(block);
}

} // namespace basalt::server::runtime::world::realBlockTags
