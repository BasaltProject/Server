#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/WindBursts.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <cmath>
#include <optional>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
namespace play = wire::protocol::play;

/// parity: `WindCharge`'s calculator pushes 1.22 times harder; the breeze's is the default one.
constexpr float kPlayerChargeKnockback = 1.22F;
constexpr double kPlayerEyes = 1.62;
constexpr double kPlayerWidth = 0.6;
constexpr double kPlayerHeight = 1.8;
/// parity: `ServerLevel.explode` tells only the players within sixty-four blocks.
constexpr double kPacketRangeSquared = 4096.0;

/// parity: `ServerExplosion.getSeenPercent`: the share of points across the box with a clear line
/// to the centre.
[[nodiscard]] float seenPercent(world::WorldState& world, double cx, double cy, double cz, const entity::Aabb& box) {
    const double xs = 1.0 / ((box.maxX - box.minX) * 2.0 + 1.0);
    const double ys = 1.0 / ((box.maxY - box.minY) * 2.0 + 1.0);
    const double zs = 1.0 / ((box.maxZ - box.minZ) * 2.0 + 1.0);
    const double xOffset = (1.0 - (std::floor(1.0 / xs) * xs)) / 2.0;
    const double zOffset = (1.0 - (std::floor(1.0 / zs) * zs)) / 2.0;
    std::int32_t hits = 0;
    std::int32_t count = 0;
    for (double xx = 0.0; xx <= 1.0; xx += xs) {
        for (double yy = 0.0; yy <= 1.0; yy += ys) {
            for (double zz = 0.0; zz <= 1.0; zz += zs) {
                const double x = box.minX + (xx * (box.maxX - box.minX));
                const double y = box.minY + (yy * (box.maxY - box.minY));
                const double z = box.minZ + (zz * (box.maxZ - box.minZ));
                if (!entity::blockRay::cast(world, x + xOffset, y, z + zOffset, cx, cy, cz).has_value()) {
                    ++hits;
                }
                ++count;
            }
        }
    }
    return count == 0 ? 0.0F : static_cast<float>(hits) / static_cast<float>(count);
}

/// parity: `ServerExplosion.hurtEntities`' push for something at `feet` with its eyes at `eyes`, or
/// nothing when it is out of reach.
[[nodiscard]] std::optional<std::array<double, 3>> pushFor(world::WorldState& world, double cx, double cy, double cz,
                                                           float radius, const std::array<double, 3>& feet, double eyes,
                                                           const entity::Aabb& box, float multiplier,
                                                           double resistance) {
    const double doubleRadius = static_cast<double>(radius * 2.0F);
    const double dx = feet[0] - cx;
    const double dy = feet[1] - cy;
    const double dz = feet[2] - cz;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) / doubleRadius;
    if (distance > 1.0) {
        return std::nullopt;
    }
    const double ex = feet[0] - cx;
    const double ey = eyes - cy;
    const double ez = feet[2] - cz;
    const double length = std::sqrt((ex * ex) + (ey * ey) + (ez * ez));
    const std::array<double, 3> direction =
        length < 1.0E-5 ? std::array<double, 3>{} : std::array<double, 3>{ex / length, ey / length, ez / length};
    const float exposure = multiplier == 0.0F ? 0.0F : seenPercent(world, cx, cy, cz, box);
    const double power =
        (1.0 - distance) * static_cast<double>(exposure) * static_cast<double>(multiplier) * (1.0 - resistance);
    return std::array<double, 3>{direction[0] * power, direction[1] * power, direction[2] * power};
}

} // namespace

bool CombatSystem::breezeProof(const std::vector<ServerEntity*>& living, const ServerEntity& charge,
                               const ServerEntity& target) {
    if (target.type().path() != "breeze") {
        return false;
    }
    for (const ServerEntity* mob : living) {
        if (mob->entityId() == charge.ownerId) {
            return mob->type().path() == "breeze";
        }
    }
    return false;
}

void CombatSystem::gustWhereItLanded(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                     const std::vector<ServerEntity*>& living, const ServerEntity& charge, double x,
                                     double y, double z) {
    const bool breeze = charge.type().path() == "breeze_wind_charge";
    const float radius = breeze ? world::windBursts::kBreezeRadius : world::windBursts::kRadius;
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    // parity: `hurtEntities` runs before the blocks: the push, which is all a wind charge's blast does
    // to what it reaches.
    const float mobMultiplier = breeze ? 1.0F : kPlayerChargeKnockback;
    for (ServerEntity* mob : living) {
        if (mob == &charge || mob->removed || mob->dying ||
            !server::registry::entityAttributes::isLiving(mob->type())) {
            continue;
        }
        const double eyes = mob->y + entity::mobBreathing::eyeHeightOf(*mob);
        if (const std::optional<std::array<double, 3>> push =
                pushFor(world.state(), x, y, z, radius, {mob->x, mob->y, mob->z}, eyes,
                        entity::Aabb::of(mob->x, mob->y, mob->z, mob->width(), mob->height()), mobMultiplier,
                        mob->attributeWithModifiers("minecraft:explosion_knockback_resistance"));
            push.has_value()) {
            mob->velocityX += (*push)[0];
            mob->velocityY += (*push)[1];
            mob->velocityZ += (*push)[2];
        }
    }
    const world::explosions::Spares stops = [this](world::WorldState&, std::int32_t, std::int32_t, std::int32_t,
                                                   std::int32_t stateId) {
        const Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
        return options_.blockTags != nullptr && block != nullptr &&
               world::realBlockTags::contains(*options_.blockTags, "minecraft:blocks_wind_charge_explosions", *block);
    };
    const std::vector<world::BlockChange> knocked =
        world::windBursts::burst(world.state(), facts.chunkHeight.minY(), facts.chunkHeight.maxY(), x, y, z,
                                 world.gameTime(), random, stops, radius);
    for (const world::BlockChange& change : knocked) {
        broadcaster_.announceRedstone(world, watchers, change);
        // parity: `updateNeighbours` tells the wall a button or a lever is stuck to, which is what
        // carries the signal into whatever is behind it.
        if (const std::optional<std::array<std::int32_t, 3>> wall =
                world::blockInteractions::attachmentOf(change.stateId, change.x, change.y, change.z);
            wall.has_value()) {
            broadcaster_.announceRedstone(
                world, watchers,
                world::BlockChange{.x = (*wall)[0],
                                   .y = (*wall)[1],
                                   .z = (*wall)[2],
                                   .stateId =
                                       world.state().blockState((*wall)[0], (*wall)[1], (*wall)[2]).value_or(0)});
        }
    }
    // parity: `ServerLevel.explode`'s packet: the gust, its burst sound, and each player's own push,
    // which the client applies to itself.
    const bool silent = charge.sourceNbt.has_value() && charge.sourceNbt->getBoolean("Silent");
    for (PlayerSession* player : watchers) {
        const double dx = player->x - x;
        const double dy = player->y - y;
        const double dz = player->z - z;
        // parity: `ServerExplosion.hurtEntities`' `onExplosionHit`, which only a player's own wind
        // charge makes an impulse that spares the fall.
        if (player->gamemode != play::Gamemode::Spectator &&
            std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) / (static_cast<double>(radius) * 2.0) <= 1.0) {
            player::impulse::onExplosionHit(*player, charge.type().path() == "wind_charge");
        }
        if ((dx * dx) + (dy * dy) + (dz * dz) >= kPacketRangeSquared) {
            continue;
        }
        play::ExplodePacket packet{
            .x = x,
            .y = y,
            .z = z,
            .radius = radius,
            .blockCount = static_cast<std::int32_t>(knocked.size()),
            .particleId = ExplosionSystem::particleId(breeze ? "gust_emitter_large" : "gust_emitter_small"),
            .sound = breeze ? "entity.breeze.wind_burst" : "entity.wind_charge.wind_burst",
            .playSound = !silent};
        const bool spectator = player->gamemode == play::Gamemode::Spectator;
        const bool creativeFlying = player->gamemode == play::Gamemode::Creative && player->flying;
        if (!spectator && !creativeFlying) {
            const float multiplier = player->flying ? 0.0F : mobMultiplier;
            if (const std::optional<std::array<double, 3>> push = pushFor(
                    world.state(), x, y, z, radius, {player->x, player->y, player->z}, player->y + kPlayerEyes,
                    entity::Aabb::of(player->x, player->y, player->z, kPlayerWidth, kPlayerHeight), multiplier, 0.0);
                push.has_value()) {
                packet.knockbackX = (*push)[0];
                packet.knockbackY = (*push)[1];
                packet.knockbackZ = (*push)[2];
            }
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::ExplodePacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

} // namespace basalt::server::runtime
