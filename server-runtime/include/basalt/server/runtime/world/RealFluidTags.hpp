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

namespace realFluidTags {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagMembers = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

/// parity: the `minecraft:fluid` registry, whose five ids are what a fluid tag carries.
[[nodiscard]] const std::map<wire::common::Identifier, std::int32_t>& protocolIds();

[[nodiscard]] TagMembers load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

[[nodiscard]] const std::set<wire::common::Identifier>& membersOf(const TagMembers& tags,
                                                                  const wire::common::Identifier& tag);

} // namespace realFluidTags

} // namespace basalt::server::runtime::world
