#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WanderingTraderLifetime.hpp>

#include <algorithm>

namespace basalt::server::runtime::entity::wanderingTraderLifetime {
namespace {

constexpr std::int32_t kLeashRestoreGrace = 100;

ServerEntity* byId(const std::vector<ServerEntity*>& entities, std::int32_t entityId) {
    for (ServerEntity* candidate : entities) {
        if (candidate->entityId() == entityId) {
            return candidate;
        }
    }
    return nullptr;
}

/// Reattaches a leash to whoever is holding it, or gives up on it once the grace is spent.
void resolveLeash(ServerEntity& entity, const std::vector<ServerEntity*>& entities) {
    if (entity.leashHolderId.has_value() && byId(entities, *entity.leashHolderId) != nullptr) {
        return;
    }
    entity.leashHolderId.reset();
    if (!entity.leashHolderUuid.has_value()) {
        return;
    }
    for (const ServerEntity* candidate : entities) {
        if (candidate->uuid().toString() == *entity.leashHolderUuid) {
            entity.leashHolderId = candidate->entityId();
            break;
        }
    }
    if (!entity.leashHolderId.has_value() && entity.tickCount > kLeashRestoreGrace) {
        entity.leashHolderUuid.reset();
        if (entity.sourceNbt.has_value()) {
            entity.sourceNbt->remove("leash");
        }
    }
}

void tickTrader(ServerEntity& trader) {
    if (trader.despawnDelay > 0) {
        trader.despawnDelay--;
        if (trader.despawnDelay == 0) {
            trader.removed = true;
        }
    }
}

void tickLlama(ServerEntity& llama, const std::vector<ServerEntity*>& entities,
               const std::set<std::int32_t>& playerIds) {
    const ServerEntity* holder = llama.leashHolderId.has_value() ? byId(entities, *llama.leashHolderId) : nullptr;
    const bool leashedToTrader = holder != nullptr && holder->type().path() == "wandering_trader";
    const bool leashedElsewhere = holder != nullptr && !leashedToTrader;
    const auto riders =
        std::ranges::count_if(llama.passengerIds, [&playerIds](std::int32_t id) { return playerIds.contains(id); });
    const bool canDespawn = !llama.owner.has_value() && !leashedElsewhere && riders != 1 && !llama.persistent;
    if (!canDespawn) {
        return;
    }
    llama.despawnDelay = leashedToTrader ? holder->despawnDelay - 1 : llama.despawnDelay - 1;
    if (llama.despawnDelay <= 0) {
        llama.leashHolderId.reset();
        llama.leashHolderUuid.reset();
        llama.removed = true;
    }
}

} // namespace

void tick(ServerEntity& entity, const std::vector<ServerEntity*>& entities, const std::set<std::int32_t>& playerIds) {
    resolveLeash(entity, entities);
    const std::string_view path = entity.type().path();
    if (path == "wandering_trader") {
        tickTrader(entity);
    } else if (path == "trader_llama") {
        tickLlama(entity, entities, playerIds);
    }
}

} // namespace basalt::server::runtime::entity::wanderingTraderLifetime
