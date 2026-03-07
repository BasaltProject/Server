#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/RealBannerPatterns.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realBannerPatterns {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

constexpr std::string_view kPrefix = "data/minecraft/tags/banner_pattern/";
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

TagOrder load(const JarReader& jar, const wire::common::Logger& logger) {
    if (!jar.entriesUnder || !jar.readText) {
        return {};
    }
    TagOrder tags;
    for (const std::string& entry : jar.entriesUnder(kPrefix)) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        const std::optional<std::string> text = jar.readText(entry);
        if (!text.has_value()) {
            logger.debug([&] { return "Skipped banner pattern tag " + entry + ": unreadable"; });
            continue;
        }
        const std::expected<reg::tags::TagFile, reg::tags::tagFileJson::ParseError> file =
            reg::tags::tagFileJson::parse(*text);
        if (!file.has_value()) {
            logger.debug([&] { return "Skipped banner pattern tag " + entry + ": malformed"; });
            continue;
        }
        std::vector<Identifier> members;
        members.reserve(file->entries.size());
        for (const reg::tags::TagEntry& listed : file->entries) {
            if (!listed.isTag) {
                members.push_back(listed.id);
            }
        }
        tags.insert_or_assign(nameOf(entry), std::move(members));
    }
    return tags;
}

const std::vector<Identifier>& membersOf(const TagOrder& tags, const std::string_view tag) {
    static const std::vector<Identifier> empty;
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(tag);
    if (!id.has_value()) {
        return empty;
    }
    const auto found = tags.find(*id);
    return found == tags.end() ? empty : found->second;
}

} // namespace basalt::server::runtime::world::realBannerPatterns
