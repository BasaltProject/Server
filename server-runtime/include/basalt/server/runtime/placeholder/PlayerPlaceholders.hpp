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

/// What `%basalt_player_*%` reads besides the session itself.
struct PlayerPlaceholderSources {
    std::function<const config::GroupsConfig&()> groups;
    /// Looks a world up by name, so `dimension` can answer without the session carrying a copy.
    std::function<const world::LoadedWorld*(std::string_view)> world;
};

/// `%basalt_player_*%`, answered from whoever the context is about.
///
/// The same names serve both situations: `%basalt_player_name%` is the player talking in a chat
/// format and whoever was recognised at that address in the server list. One that only one
/// situation can answer says so rather than inventing a value.
[[nodiscard]] wire::common::placeholder::PlaceholderProvider playerPlaceholders(PlayerPlaceholderSources sources);

[[nodiscard]] wire::common::placeholder::PlaceholderCatalog playerPlaceholderNames();

/// parity: the compass directions vanilla's F3 screen shows, from the same yaw ranges.
[[nodiscard]] std::string_view compassDirection(float yaw);

/// The same, rounded to the four cardinal points.
[[nodiscard]] std::string_view compassDirectionXZ(float yaw);

/// `mined_stone` to `minecraft:mined/minecraft:stone`, and `deaths` to
/// `minecraft:custom/minecraft:deaths`.
///
/// A key already carrying a namespace is left alone, so anything vanilla can name can still be
/// named in full.
[[nodiscard]] std::string expandStatKey(std::string_view key);

} // namespace basalt::server::runtime::placeholder
