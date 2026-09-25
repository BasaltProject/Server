#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/SummonAwards.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>

namespace basalt::server::runtime::world::summonAwards {

namespace reg = ::basalt::server::registry;

void award(AdvancementTriggers* triggers, const std::vector<player::PlayerSession*>& players,
           std::string_view worldName, const entity::ServerEntity& summoned, double range) {
    if (triggers == nullptr) {
        return;
    }
    const entity::Aabb around =
        entity::Aabb::of(summoned.x, summoned.y, summoned.z, reg::entityTypeShapes::widthOf(summoned.type()),
                         reg::entityTypeShapes::heightOf(summoned.type()))
            .inflate(range);
    for (player::PlayerSession* player : players) {
        if (player == nullptr || player->worldName != worldName) {
            continue;
        }
        const entity::Aabb theirs = entity::Aabb::of(player->x, player->y, player->z, blockPlacement::kPlayerWidth,
                                                     blockPlacement::kPlayerHeight);
        if (around.intersects(theirs)) {
            triggers->fire("summoned_entity", player->name(), player->uuid().toString(),
                           TriggerEvent{.entities = {{"entity", triggerFacts::ofEntity(summoned)}}});
        }
    }
}

} // namespace basalt::server::runtime::world::summonAwards
