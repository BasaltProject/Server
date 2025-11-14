#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

/// Every tag name vanilla 26.2 ships, grouped by the registry it tags.
///
/// Names alone unblock Configuration, since the client's bootstrap throws on an unknown tag key.
/// Members are another matter: the client tests `FluidTags.WATER` before it renders as under water,
/// and tool rules read the block tags. Block, item, fluid and entity type members come from the
/// operator's own jar; the rest still go out empty.
///
/// Only registries the client tracks are here, meaning the `BuiltInRegistries` ones plus
/// `RegistryDataLoader.SYNCHRONIZED_REGISTRIES`. Anything else is absent from the client's
/// `RegistryAccess`: sending tags for `worldgen/world_preset` crashed it with `Missing registry`
/// before any tag lookup ran.
namespace vanillaTagData {

/// In the order the registries are declared, which is the order they go out on the wire.
[[nodiscard]] const std::vector<std::pair<std::string, std::vector<std::string>>>& tagsByRegistry();

/// The tags of one registry, empty when this build does not track it.
[[nodiscard]] const std::vector<std::string>& tagsOf(std::string_view registry);

} // namespace vanillaTagData

} // namespace basalt::server::runtime::registry
