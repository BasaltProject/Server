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

/// The enchantment tag graph out of the operator's own `server.jar`, flattened once at startup.
///
/// What an anvil's `exclusive_set` and a villager's `tradeable` resolve through. Datapack content,
/// so it is read from the download rather than committed, the same as the item tags.
namespace realEnchantmentTags {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

/// Whether an enchantment of that name exists, which is what an unresolvable entry is dropped by.
using EnchantmentExists = std::function<bool(const wire::common::Identifier& id)>;

/// Reads and flattens every enchantment tag file in the jar. Empty when there is no jar, and empty
/// again when the graph does not resolve.
[[nodiscard]] TagMembers load(const JarReader& jar, const EnchantmentExists& exists,
                              const wire::common::Logger& logger = wire::common::Logger{"Tags"});

/// Everything in a tag, by its full `namespace:path` name. Empty for one that is unknown.
[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags, std::string_view tag);

[[nodiscard]] bool contains(const TagMembers& tags, std::string_view tag, const wire::common::Identifier& enchantment);

} // namespace realEnchantmentTags

} // namespace basalt::server::runtime::world
