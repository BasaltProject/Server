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

/// The block tag graph out of the operator's own `server.jar`, flattened once at startup.
///
/// This is what turns a tool's rules into an answer: a rule names `minecraft:mineable/pickaxe` or
/// `minecraft:incorrect_for_diamond_tool`, and only the real tag files say which blocks are in them.
/// Datapack content, so it is read from the download rather than committed, the same as loot tables
/// and recipes.
namespace realBlockTags {

/// The jar's directory listing and its file reader, as callbacks rather than the jar itself: this is
/// the only thing here that needs the operator's own copy of the game, and a test has none to give.
struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

/// Every tag's members, by the tag's own name.
using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

/// Reads and flattens every block tag file in the jar. Empty when there is no jar, and empty again
/// when the graph does not resolve: a broken tag file leaves tool requirements unapplied rather than
/// taking the server down with it.
[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

/// Everything in a tag, by the tag's full `namespace:path` name. Empty for one that is unknown.
[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags, std::string_view tag);

/// Whether a block is in a tag, by the tag's full `namespace:path` name.
[[nodiscard]] bool contains(const TagMembers& tags, std::string_view tag, const wire::common::Identifier& block);

} // namespace realBlockTags

} // namespace basalt::server::runtime::world
