#pragma once

#include <basalt/server/plugins/PluginEntityOps.hpp>

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {
struct ModelHooks;
}

namespace basalt::server::runtime::plugins::entityOps {

struct Environment {
    const ModelHooks* hooks = nullptr;
    world::LoadedWorld* world = nullptr;
};

[[nodiscard]] std::optional<server::plugins::PluginEntityValue>
get(const Environment& environment, const entity::ServerEntity& entity, std::string_view name);

bool set(const Environment& environment, entity::ServerEntity& entity, std::string_view name,
         const server::plugins::PluginEntityValue& value);

std::optional<server::plugins::PluginEntityValue>
invoke(const Environment& environment, entity::ServerEntity& entity, std::string_view operation,
       const std::vector<server::plugins::PluginEntityValue>& arguments);

} // namespace basalt::server::runtime::plugins::entityOps
