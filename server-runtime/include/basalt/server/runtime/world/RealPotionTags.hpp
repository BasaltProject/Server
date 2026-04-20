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

/// Potion ids have no registry in this build, so a tag entry binds by name alone, unlike item and fluid tags.
namespace realPotionTags {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags,
                                                                  const wire::common::Identifier& tag);

} // namespace realPotionTags

} // namespace basalt::server::runtime::world
