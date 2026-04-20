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

/// What an anvil's `exclusive_set` and a villager's `tradeable` resolve through.
namespace realEnchantmentTags {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

using EnchantmentExists = std::function<bool(const wire::common::Identifier& id)>;

[[nodiscard]] TagMembers load(const JarReader& jar, const EnchantmentExists& exists,
                              const wire::common::Logger& logger = wire::common::Logger{"Tags"});

[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags, std::string_view tag);

[[nodiscard]] bool contains(const TagMembers& tags, std::string_view tag, const wire::common::Identifier& enchantment);

} // namespace realEnchantmentTags

} // namespace basalt::server::runtime::world
