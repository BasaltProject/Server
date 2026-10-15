#pragma once

#include <cstdint>
#include <set>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace wanderingTraderLifetime {

void tick(ServerEntity& entity, const std::vector<ServerEntity*>& entities, const std::set<std::int32_t>& playerIds);

} // namespace wanderingTraderLifetime

} // namespace basalt::server::runtime::entity
