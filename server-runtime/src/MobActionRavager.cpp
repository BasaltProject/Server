#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ShieldBlows.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;

/// parity: `Ravager.roar`, six points to everything within four blocks of its box.
constexpr float kRoarDamage = 6.0F;
constexpr double kRoarReach = 4.0;
/// parity: `Ravager.doHurtTarget` and its entity events 4 and 69.
constexpr std::int32_t kAttackTicks = 10;
constexpr std::int8_t kAttackEvent = 4;
constexpr std::int8_t kRoarEvent = 69;
constexpr std::string_view kDeathMob = "mob";

/// parity: `AbstractIllager`, which a roar leaves unhurt.
constexpr std::array<std::string_view, 4> kIllagers{"pillager", "vindicator", "evoker", "illusioner"};

void sendEvent(const std::vector<PlayerSession*>& watchers, const ServerEntity& mob, std::int8_t event) {
    const wire::protocol::play::EntityEventPacket packet{.entityId = mob.entityId(), .eventId = event};
    for (PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

[[nodiscard]] bool inRoar(const ServerEntity& ravager, double x, double y, double z, double width, double height) {
    const double half = (ravager.width() / 2.0) + kRoarReach + (width / 2.0);
    return std::abs(x - ravager.x) < half && std::abs(z - ravager.z) < half && y + height > ravager.y - kRoarReach &&
           y < ravager.y + ravager.height() + kRoarReach;
}

} // namespace

void MobActionSystem::ravagerSwings(world::LoadedWorld& world, ServerEntity& ravager) {
    ravager.ravagerAttackTicks = kAttackTicks;
    sendEvent(watchersOf(world), ravager, kAttackEvent);
    playSound(world, ravager, "entity.ravager.attack");
}

void MobActionSystem::tickRavager(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  ServerEntity& ravager) {
    if (ravager.dying || ravager.removed) {
        return;
    }
    const entity::shieldBlows::RavagerTurn turn =
        entity::shieldBlows::tickRavager(ravager, ravager.targetId.has_value());
    if (turn == entity::shieldBlows::RavagerTurn::StunEnds) {
        playSound(world, ravager, "entity.ravager.roar");
        return;
    }
    if (turn != entity::shieldBlows::RavagerTurn::Roars) {
        return;
    }
    // parity: `Ravager.roar`.
    const bool griefing =
        entity::mobOverrides::griefing(ravager, world.gameRules().get(world::gameRules::kMobGriefing));
    for (ServerEntity* other : world.state().entities()) {
        if (other == &ravager || other->removed || other->dying || other->type().path() == "ravager" ||
            !server::registry::entityAttributes::isLiving(other->type()) ||
            (!griefing && other->type().path() == "armor_stand") ||
            !inRoar(ravager, other->x, other->y, other->z, other->width(), other->height())) {
            continue;
        }
        if (std::ranges::find(kIllagers, other->type().path()) == kIllagers.end()) {
            entity::entityCombat::blame(*other, ravager.entityId(), ravager.entityId(), ravager.type());
            const entity::entityCombat::Result result = entity::entityCombat::hurt(
                *other, kRoarDamage, ravager.x, ravager.z, hooks_.combat, "mob_attack", ravager.type().toString());
            if (result.landed) {
                combat_->announceEntityHit(watchers, *other, world::damageTypes::kMobAttack, ravager.entityId(),
                                           result.died);
            }
        }
        const entity::shieldBlows::Push push = entity::shieldBlows::strongKnockback(ravager, other->x, other->z);
        other->velocityX += push.x;
        other->velocityY += push.y;
        other->velocityZ += push.z;
    }
    for (PlayerSession* player : watchers) {
        if (player->dead || !player->connection().hurt || !inRoar(ravager, player->x, player->y, player->z, 0.6, 1.8)) {
            continue;
        }
        player->connection().hurt(kRoarDamage, kDeathMob, world::damageTypes::kMobAttack,
                                  entity::DamageSource{.entityId = ravager.entityId(),
                                                       .x = ravager.x,
                                                       .z = ravager.z,
                                                       .name = entity::entityCombat::displayName(ravager)});
    }
    world::vibrations::emit(world.state(), "entity_action", ravager.x, ravager.y, ravager.z, world.gameTime(),
                            {.entity = ravager.uuid()});
    sendEvent(watchers, ravager, kRoarEvent);
}

} // namespace basalt::server::runtime
