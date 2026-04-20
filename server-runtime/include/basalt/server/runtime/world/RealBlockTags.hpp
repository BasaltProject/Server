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

/// Datapack content, read from the download rather than committed; a rule's tag only resolves through the real files.
namespace realBlockTags {

/// Callbacks rather than the jar itself: only this needs the operator's copy of the game, and a test has none.
struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags, std::string_view tag);

[[nodiscard]] bool contains(const TagMembers& tags, std::string_view tag, const wire::common::Identifier& block);

} // namespace realBlockTags

} // namespace basalt::server::runtime::world
