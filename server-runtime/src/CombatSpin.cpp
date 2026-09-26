#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;

} // namespace
} // namespace basalt::server::runtime

namespace basalt::server::runtime {

void CombatHandler::spinAttack(PlayerSession& session, double toX, double toY, double toZ) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || session.autoSpinTicks <= 0) {
        return;
    }
    // parity: `LivingEntity.checkAutoSpinAttack` over the box swept from where it was to where it is,
    // the spin attack pose being six tenths of a block every way.
    constexpr double kHalf = 0.3;
    constexpr double kHeight = 0.6;
    const double minX = std::min(session.x, toX) - kHalf;
    const double maxX = std::max(session.x, toX) + kHalf;
    const double minY = std::min(session.y, toY);
    const double maxY = std::max(session.y, toY) + kHeight;
    const double minZ = std::min(session.z, toZ) - kHalf;
    const double maxZ = std::max(session.z, toZ) + kHalf;
    for (ServerEntity* candidate : world->state().entities()) {
        const double half = candidate->width() / 2.0;
        if (candidate->removed || candidate->dying ||
            !server::registry::entityAttributes::isLiving(candidate->type()) || candidate->x + half <= minX ||
            candidate->x - half >= maxX || candidate->y + candidate->height() <= minY || candidate->y >= maxY ||
            candidate->z + half <= minZ || candidate->z - half >= maxZ) {
            continue;
        }
        spinning_ = true;
        handleAttack(wire::protocol::play::AttackPacket{.entityId = candidate->entityId()});
        spinning_ = false;
        session.autoSpinTicks = 0;
        break;
    }
    if (session.autoSpinTicks <= 0) {
        session.autoSpinDamage = 0.0F;
        session.autoSpinItem.reset();
    }
}

} // namespace basalt::server::runtime
