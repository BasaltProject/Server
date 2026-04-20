#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <functional>
#include <map>
#include <string>

namespace basalt::server::runtime::config {
class GroupsConfig;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::placeholder {

struct PlayerPlaceholderSources {
    std::function<const config::GroupsConfig&()> groups;
    std::function<const world::LoadedWorld*(std::string_view)> world;
};

[[nodiscard]] wire::common::placeholder::PlaceholderProvider playerPlaceholders(PlayerPlaceholderSources sources);

[[nodiscard]] wire::common::placeholder::PlaceholderCatalog playerPlaceholderNames();

/// parity: the compass directions vanilla's F3 screen shows, from the same yaw ranges.
[[nodiscard]] std::string_view compassDirection(float yaw);

[[nodiscard]] std::string_view compassDirectionXZ(float yaw);

[[nodiscard]] std::string expandStatKey(std::string_view key);

} // namespace basalt::server::runtime::placeholder
