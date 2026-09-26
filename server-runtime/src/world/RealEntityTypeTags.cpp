#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>

#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realEntityTypeTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

} // namespace

TagMembers load(const JarReader& jar, const wire::common::Logger& logger) {
    return datapack::resolveTags(
        jar.entriesUnder, jar.readText, "entity_type",
        [](const Identifier& id) { return reg::entityTypes::of(id) != nullptr; }, logger);
}

const std::set<Identifier>& membersOf(const TagMembers& tags, const Identifier& tag) {
    static const std::set<Identifier> empty;
    const auto found = tags.find(tag);
    return found == tags.end() ? empty : found->second;
}

} // namespace basalt::server::runtime::world::realEntityTypeTags
