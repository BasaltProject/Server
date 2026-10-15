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

/// What a recipe's `#minecraft:planks` and a tooltip's `#minecraft:swords` resolve through.
namespace realItemTags {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

/// A broken tag file leaves tag-based recipes unmatched rather than failing startup.
[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags, std::string_view tag);

[[nodiscard]] bool contains(const TagMembers& tags, std::string_view tag, const wire::common::Identifier& item);

} // namespace realItemTags

} // namespace basalt::server::runtime::world
