#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands::itemSlots {

[[nodiscard]] std::optional<std::int32_t> resolve(const std::string& name, bool container = false);

[[nodiscard]] const std::vector<std::string>& names();

[[nodiscard]] Parsed<std::int32_t> slotOf(const CommandContext& context, std::string_view name, bool container = false);

[[nodiscard]] std::vector<std::pair<std::int32_t, storage::StoredItemStack>>
containerItems(const CommandContext& context, const std::array<std::int32_t, 3>& at);

[[nodiscard]] std::optional<CommandSyntaxError>
writeContainer(const CommandContext& context, const std::array<std::int32_t, 3>& at,
               const std::vector<std::pair<std::int32_t, storage::StoredItemStack>>& written,
               world::RealRecipes* recipes = nullptr);

} // namespace basalt::server::runtime::command::commands::itemSlots
