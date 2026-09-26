#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealFluidTags.hpp>

#include <cstdint>
#include <expected>
#include <utility>

namespace basalt::server::runtime::world::realFluidTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

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
    return datapack::resolveTags(
        jar.entriesUnder, jar.readText, "fluid", [](const Identifier& id) { return protocolIds().contains(id); },
        logger);
}

const std::set<Identifier>& membersOf(const TagMembers& tags, const Identifier& tag) {
    static const std::set<Identifier> empty;
    const auto found = tags.find(tag);
    return found == tags.end() ? empty : found->second;
}

} // namespace basalt::server::runtime::world::realFluidTags
