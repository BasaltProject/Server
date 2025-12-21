#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// Slot names and container access, shared by `/item` and `/loot`: both address the same slots and
/// both write into the same `Items` list of a container's block entity.
namespace basalt::server::runtime::command::commands::itemSlots {

/// This server's own index for a vanilla slot name, or nothing when nothing here has that slot.
///
/// parity: `SlotArgument`'s names. Only a player's own slots are here, because a container's slots
/// need block entities with inventories, which do not exist yet.
[[nodiscard]] std::optional<std::int32_t> resolve(const std::string& name);

/// Every name `resolve` answers, for completions. The numbered ones are listed in full.
[[nodiscard]] const std::vector<std::string>& names();

/// The slot a named argument holds, or why the name is not one.
[[nodiscard]] Parsed<std::int32_t> slotOf(const CommandContext& context, std::string_view name);

/// What a container holds, by slot. An empty answer covers both "no container" and "empty one":
/// every caller treats them the same way, as a slot that is not there.
[[nodiscard]] std::vector<std::pair<std::int32_t, storage::StoredItemStack>>
containerItems(const CommandContext& context, const std::array<std::int32_t, 3>& at);

/// Writes stacks into a container's own `Items`, keeping whatever else its NBT carries.
///
/// `replaced` slots are dropped first, which is what makes a write a replace rather than a second
/// entry for the same slot.
[[nodiscard]] std::optional<CommandSyntaxError>
writeContainer(const CommandContext& context, const std::array<std::int32_t, 3>& at,
               const std::vector<std::pair<std::int32_t, storage::StoredItemStack>>& written);

} // namespace basalt::server::runtime::command::commands::itemSlots
