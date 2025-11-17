#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/world/RealFluidTags.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realFluidTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

constexpr std::string_view kPrefix = "data/minecraft/tags/fluid/";
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

const std::map<Identifier, std::int32_t>& protocolIds() {
    static const std::map<Identifier, std::int32_t> ids{
        {Identifier::vanilla("empty"), 0}, {Identifier::vanilla("flowing_water"), 1},
        {Identifier::vanilla("water"), 2}, {Identifier::vanilla("flowing_lava"), 3},
        {Identifier::vanilla("lava"), 4},
    };
    return ids;
}

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
            logger.debug([&] { return "Skipped fluid tag " + entry + ": unreadable"; });
            continue;
        }
        const std::expected<reg::tags::TagFile, reg::tags::tagFileJson::ParseError> file =
            reg::tags::tagFileJson::parse(*text);
        if (!file.has_value()) {
            logger.debug([&] { return "Skipped fluid tag " + entry + ": malformed"; });
            continue;
        }
        files.insert_or_assign(nameOf(entry), *file);
    }
    std::expected<TagMembers, reg::tags::TagError> resolved =
        reg::tags::resolve(files, [](const Identifier& id) { return protocolIds().contains(id); });
    if (!resolved.has_value()) {
        logger.warn([&] {
            return "Fluid tags could not be resolved, so nothing counts as water: " + resolved.error().message();
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

} // namespace basalt::server::runtime::world::realFluidTags
