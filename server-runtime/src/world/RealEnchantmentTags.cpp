#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealEnchantmentTags.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realEnchantmentTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

} // namespace

TagMembers load(const JarReader& jar, const EnchantmentExists& exists, const wire::common::Logger& logger) {
    if (!exists) {
        return {};
    }
    return datapack::resolveTags(jar.entriesUnder, jar.readText, "enchantment", exists, logger);
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

bool contains(const TagMembers& tags, std::string_view tag, const Identifier& enchantment) {
    return membersOf(tags, tag).contains(enchantment);
}

} // namespace basalt::server::runtime::world::realEnchantmentTags
