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

/// The potion tag graph out of the operator's own `server.jar` and packs.
///
/// What a brewing recipe's `potion_contents.potions` `#tag` resolves through. Potion ids have no
/// registry in this build, so a tag entry binds by name alone, unlike the item and fluid tags.
namespace realPotionTags {

/// The jar's directory listing and its file reader, as callbacks rather than the jar itself: this is
/// the only thing here that needs the operator's own copy of the game, and a test has none to give.
struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

/// Every tag's members, by the tag's own name.
using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

/// Reads and flattens every potion tag file. Empty when there is no jar, and empty again when the
/// graph does not resolve.
[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

/// Everything in a tag, by its full `namespace:path` name. Empty for one that is unknown.
[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags,
                                                                  const wire::common::Identifier& tag);

} // namespace realPotionTags

} // namespace basalt::server::runtime::world
