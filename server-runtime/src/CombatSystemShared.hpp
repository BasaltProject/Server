#pragma once

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WindBursts.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <utility>

namespace basalt::server::runtime::combatSystem {

/// parity: `LevelEvent.PARTICLES_SPELL_POTION_SPLASH`.
constexpr std::int32_t kSplashPotionEvent = 2002;
constexpr std::int32_t kWaterBottleColour = -13083194;
constexpr double kLevelEventRange = 64.0;

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::text::Component;

/// parity: `SmallFireball.onHitEntity`, five points and five seconds of burning.
constexpr float kSmallFireballDamage = 5.0F;
constexpr std::int32_t kSmallFireballFireTicks = 100;
constexpr std::string_view kDeathOnFire = "onFire";

/// parity: `AbstractWindCharge.onHitEntity`.
constexpr float kWindChargeDamage = 1.0F;
constexpr std::string_view kDeathWindCharge = "mob";

/// parity: `applySplash`, four blocks across and twenty seconds at the centre.
constexpr double kSplashRange = 4.0;
constexpr double kSplashDuration = 400.0;
constexpr std::string_view kSoundSplash = "entity.splash_potion.break";

constexpr std::string_view kSoundTeleport = "entity.player.teleport";
constexpr std::string_view kDeathFall = "fall";

/// parity: `ThrownEnderpearl.onHit`, five points of fall damage on arrival.
constexpr float kEnderPearlDamage = 5.0F;

/// parity: `EntityType.PLAYER`.
inline const Identifier& playerType() {
    static const Identifier kType = Identifier::vanilla("player");
    return kType;
}

inline std::optional<double> clip(const ServerEntity& arrow, double fromX, double fromY, double fromZ, double x,
                                  double y, double z, double width, double height, double margin) {
    const double half = (width / 2.0) + margin;
    return entity::projectiles::clipBox(fromX, fromY, fromZ, arrow.x, arrow.y, arrow.z, x - half, y - margin, z - half,
                                        x + half, y + height + margin, z + half);
}

} // namespace basalt::server::runtime::combatSystem
