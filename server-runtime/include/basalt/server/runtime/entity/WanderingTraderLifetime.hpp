#pragma once

#include <cstdint>
#include <set>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The explicit countdowns used instead of ordinary distance despawning by traders and their llamas.
namespace wanderingTraderLifetime {

void tick(ServerEntity& entity, const std::vector<ServerEntity*>& entities, const std::set<std::int32_t>& playerIds);

} // namespace wanderingTraderLifetime

} // namespace basalt::server::runtime::entity
