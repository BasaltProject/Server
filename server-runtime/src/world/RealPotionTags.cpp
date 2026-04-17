#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealPotionTags.hpp>

#include <utility>

namespace basalt::server::runtime::world::realPotionTags {
namespace {

using wire::common::Identifier;

} // namespace

TagMembers load(const JarReader& jar, const wire::common::Logger& logger) {
    // parity: `TagLoader.loadTagsForExistingRegistries` for the potion registry. The jar ships no
    // potion tag, so nothing but a datapack ever resolves here; entries bind by name, as there is no
    // potion registry in this build to check them against.
    const registry::tags::ElementExists exists = [](const Identifier&) { return true; };
    return datapack::resolveTags(jar.entriesUnder, jar.readText, "potion", exists, logger);
}

const std::set<Identifier>& membersOf(const TagMembers& tags, const Identifier& tag) {
    static const std::set<Identifier> empty;
    const auto found = tags.find(tag);
    return found == tags.end() ? empty : found->second;
}

} // namespace basalt::server::runtime::world::realPotionTags
