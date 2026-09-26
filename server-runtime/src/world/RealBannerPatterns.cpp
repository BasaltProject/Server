#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealBannerPatterns.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realBannerPatterns {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

} // namespace

TagOrder load(const JarReader& jar, const wire::common::Logger& logger) {
    // parity: resolved in file order, which is what numbers a loom's buttons.
    return datapack::resolveTagsOrdered(
        jar.entriesUnder, jar.readText, "banner_pattern", [](const Identifier&) { return true; }, logger);
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
