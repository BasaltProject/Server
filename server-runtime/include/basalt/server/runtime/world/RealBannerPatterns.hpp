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

namespace realBannerPatterns {

struct JarReader {
    std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
    std::function<std::optional<std::string>(std::string_view name)> readText;
};

using TagOrder = std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>;

[[nodiscard]] TagOrder load(const JarReader& jar, const wire::common::Logger& logger = wire::common::Logger{"Tags"});

[[nodiscard]] const std::vector<wire::common::Identifier>& membersOf(const TagOrder& tags, std::string_view tag);

} // namespace realBannerPatterns

} // namespace basalt::server::runtime::world
