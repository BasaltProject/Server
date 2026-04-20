#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `LoomMenu`.
namespace looms {

inline constexpr std::size_t kMaxLayers = 6;

/// parity: `DataComponents.DYE`.
[[nodiscard]] std::optional<std::string> dyeColourOf(std::string_view item);

/// parity: `PROVIDES_BANNER_PATTERNS`.
[[nodiscard]] std::optional<std::string> patternTagOf(std::string_view item);

[[nodiscard]] bool isBanner(std::string_view item);

[[nodiscard]] std::size_t layersOn(const storage::StoredItemStack& banner);

/// parity: `LoomMenu.setupResultSlot` copies the banner.
[[nodiscard]] std::optional<storage::StoredItemStack> weave(const storage::StoredItemStack& banner,
                                                            std::string_view pattern, std::string_view colour);

} // namespace looms

} // namespace basalt::server::runtime::world
