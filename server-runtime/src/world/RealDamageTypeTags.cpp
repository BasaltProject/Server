#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realDamageTypeTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

} // namespace

TagMembers load(const JarReader& jar, const wire::common::Logger& logger) {
    return datapack::resolveTags(
        jar.entriesUnder, jar.readText, "damage_type", [](const Identifier&) { return true; }, logger);
}

const std::set<Identifier>& membersOf(const TagMembers& tags, const Identifier& tag) {
    static const std::set<Identifier> empty;
    const auto found = tags.find(tag);
    return found == tags.end() ? empty : found->second;
}

bool isIn(const TagMembers& tags, const Identifier& tag, const Identifier& damageType) {
    return membersOf(tags, tag).contains(damageType);
}

} // namespace basalt::server::runtime::world::realDamageTypeTags
