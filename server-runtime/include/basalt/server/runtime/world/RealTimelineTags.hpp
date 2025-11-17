#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The timeline tag graph out of the operator's own `server.jar`.
///
/// A dimension does not name its timelines one by one: it names a tag of them
/// (`#minecraft:in_overworld`), and the client draws the sky from whatever that tag resolves to.
/// Sent empty, as it was, the clock ticks and nothing reads it, which is exactly what a fixed midday
/// sky looks like.
namespace realTimelineTags {

/// The jar's directory listing and its file reader, as callbacks rather than the jar itself: this is
/// the only thing here that needs the operator's own copy of the game, and a test has none to give.
struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

/// Every tag's members, by the tag's own name.
using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

/// Reads and flattens every timeline tag file in the jar. Empty when there is no jar, and empty
/// again when the graph does not resolve, which is a sky that does not move.
[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

/// Everything in a tag, or empty for one nobody declared.
[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags,
                                                                  const wire::common::Identifier& tag);

} // namespace realTimelineTags

} // namespace basalt::server::runtime::world
