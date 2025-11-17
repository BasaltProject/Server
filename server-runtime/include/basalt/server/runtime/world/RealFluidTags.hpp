#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The fluid tag graph out of the operator's own `server.jar`.
///
/// The client asks these, not the block, before it will believe it is under water:
/// `Camera.getFluidInCamera` tests `FluidState.is(FluidTags.WATER)`, and `EntityFluidInteraction`
/// keys its trackers on the same two tags. Sent empty, as they were, water renders and nothing else
/// about it works: no fog, no overlay, no ambient sound, no air bar, no swimming.
namespace realFluidTags {

/// The jar's directory listing and its file reader, as callbacks rather than the jar itself: this is
/// the only thing here that needs the operator's own copy of the game, and a test has none to give.
struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

/// Every tag's members, by the tag's own name.
using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

/// parity: the `minecraft:fluid` registry, whose five ids are what a fluid tag carries.
[[nodiscard]] const std::map<wire::common::Identifier, std::int32_t>& protocolIds();

/// Reads and flattens every fluid tag file in the jar. Empty when there is no jar, and empty again
/// when the graph does not resolve, which is a world where nothing counts as water.
[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

/// Everything in a tag. Empty for one that is unknown.
[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags,
                                                                  const wire::common::Identifier& tag);

} // namespace realFluidTags

} // namespace basalt::server::runtime::world
