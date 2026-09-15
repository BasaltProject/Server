#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// What a loom does to a banner. parity: `LoomMenu`.
///
/// The screen's own machinery (which patterns it may offer, which one is picked) belongs to the
/// window; what is here is the arithmetic that has no window in it.
namespace looms {

/// parity: `LoomMenu.slotsChanged` refuses a banner that already carries six layers.
inline constexpr std::size_t kMaxLayers = 6;

/// The colour that dye dyes with, by its own name. parity: `DataComponents.DYE`, which every one of
/// the sixteen carries and nothing else does.
[[nodiscard]] std::optional<std::string> dyeColourOf(std::string_view item);

/// The banner-pattern tag that pattern item offers. parity: `PROVIDES_BANNER_PATTERNS`, which every
/// one of them sets to `pattern_item/` and its own name without the suffix.
[[nodiscard]] std::optional<std::string> patternTagOf(std::string_view item);

/// Whether that is a banner at all, which is what the first slot takes.
[[nodiscard]] bool isBanner(std::string_view item);

/// How many layers are already on it.
[[nodiscard]] std::size_t layersOn(const storage::StoredItemStack& banner);

/// The banner with one more layer on it, or nothing when it is already full.
///
/// parity: `LoomMenu.setupResultSlot`, which copies the banner, keeps one of it and adds the layer.
[[nodiscard]] std::optional<storage::StoredItemStack> weave(const storage::StoredItemStack& banner,
                                                            std::string_view pattern, std::string_view colour);

} // namespace looms

} // namespace basalt::server::runtime::world
