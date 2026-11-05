#pragma once

#include <basalt/server/runtime/plugins/EntityOps.hpp>

namespace basalt::server::runtime::plugins::entityOps::mobBrainOps {

std::optional<server::plugins::PluginEntityValue>
invoke(const Environment& environment, entity::ServerEntity& entity, std::string_view operation,
       const std::vector<server::plugins::PluginEntityValue>& arguments);

} // namespace basalt::server::runtime::plugins::entityOps::mobBrainOps
