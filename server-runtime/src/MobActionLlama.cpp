#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using wire::common::Identifier;
using wire::protocol::play::SoundSource;

namespace mth = wire::common::mth;

/// parity: `Llama.spit`.
constexpr double kSpitLift = static_cast<double>(0.2F);
constexpr float kSpitPower = 1.5F;
constexpr float kSpitUncertainty = 10.0F;
constexpr double kSpitDrop = static_cast<double>(0.1F);
/// parity: `target.getY(0.3333333333333333)`, a third of the way up whatever it spits at.
constexpr double kTargetShare = 0.3333333333333333;

} // namespace

void MobActionSystem::llamaSpit(ServerEntity& llama, double targetX, double targetY, double targetZ,
                                double targetHeight) {
    world::LoadedWorld* world = worldOf(llama);
    const entity::projectiles::Kind* kind = entity::projectiles::kindOf(Identifier::vanilla("llama_spit"));
    const server::registry::EntityTypeEntry* entry =
        kind == nullptr ? nullptr : server::registry::entityTypes::of(kind->type);
    if (world == nullptr || entry == nullptr) {
        return;
    }
    // parity: `new LlamaSpit(level, llama)`.
    const float reach = static_cast<float>(llama.width()) + 1.0F;
    const float body = llama.yaw * (static_cast<float>(std::numbers::pi) / 180.0F);
    const double x = llama.x - (static_cast<double>(reach) * 0.5 * static_cast<double>(mth::sin(body)));
    const double y = llama.y + entity::mobBreathing::eyeHeightOf(llama) - kSpitDrop;
    const double z = llama.z + (static_cast<double>(reach) * 0.5 * static_cast<double>(mth::cos(body)));
    const double dx = targetX - llama.x;
    const double dy = targetY + (targetHeight * kTargetShare) - y;
    const double dz = targetZ - llama.z;
    const double lift = std::sqrt((dx * dx) + (dz * dz)) * kSpitLift;
    random::RandomSource random = freshRandom();
    ServerEntity& spit =
        entity::projectiles::spawn(world->state(), *kind, players_->allocateEntityId(), entry->protocolId, x, y, z, dx,
                                   dy + lift, dz, kSpitPower, kSpitUncertainty, random);
    spit.ownerId = llama.entityId();
    spit.ownerUuid = llama.uuid();
    // parity: `Projectile.getAddEntityPacket`.
    spit.objectData = llama.entityId();
    if (!entity::horses::silent(llama)) {
        const float pitch = 1.0F + ((random.nextFloat() - random.nextFloat()) * 0.2F);
        playSound(*world, llama, "entity.llama.spit", 1.0F, pitch, SoundSource::Neutral);
    }
    llama.didSpit = true;
}

} // namespace basalt::server::runtime
