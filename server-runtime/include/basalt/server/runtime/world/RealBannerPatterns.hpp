#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The banner-pattern tags out of the operator's own `server.jar`, kept in the order their files
/// list them.
///
/// Order is the whole point: a loom's buttons are numbered by where a pattern sits in the tag, so a
/// list sorted by name would weave a different pattern from the one the player pressed.
namespace realBannerPatterns {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagOrder = std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>;

/// Reads every banner-pattern tag file in the jar. These name no other tags, so nothing is
/// flattened: what the file lists is what the tag holds.
[[nodiscard]] TagOrder load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

/// Everything in a tag, by its full `namespace:path` name. Empty for one that is unknown.
[[nodiscard]] const std::vector<wire::common::Identifier>& membersOf(const TagOrder& tags, std::string_view tag);

} // namespace realBannerPatterns

} // namespace basalt::server::runtime::world
