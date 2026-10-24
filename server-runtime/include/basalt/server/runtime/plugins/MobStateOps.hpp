#pragma once

#include <basalt/server/runtime/plugins/EntityOps.hpp>

namespace basalt::server::runtime::plugins::entityOps::mobState {

[[nodiscard]] std::optional<server::plugins::PluginEntityValue>
get(const Environment& environment, const entity::ServerEntity& entity, std::string_view name);

/// Empty when the name is not a mob state property; otherwise whether the write took.
[[nodiscard]] std::optional<bool> set(const Environment& environment, entity::ServerEntity& entity,
                                      std::string_view name, const server::plugins::PluginEntityValue& value);

std::optional<server::plugins::PluginEntityValue>
invoke(const Environment& environment, entity::ServerEntity& entity, std::string_view operation,
       const std::vector<server::plugins::PluginEntityValue>& arguments);

} // namespace basalt::server::runtime::plugins::entityOps::mobState
