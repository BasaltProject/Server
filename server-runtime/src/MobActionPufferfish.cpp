#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime {
namespace {

using entity::Aabb;
using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;

/// parity: `PufferfishPuffGoal`'s box.
constexpr double kScareInflate = 2.0;
constexpr double kAuraInflate = 0.3;
/// parity: `Pufferfish.tick`.
constexpr std::int32_t kFullPuffAt = 40;
constexpr std::int32_t kHalfDeflatedAfter = 60;
constexpr std::int32_t kDeflatedAfter = 100;
/// parity: `Pufferfish.touch` and `playerTouch`, one heart per puff level.
constexpr float kStingPerPuff = 1.0F;
/// parity: `Pufferfish.touch`, a second of poison per puff level.
constexpr std::int32_t kPoisonPerPuff = 60;
/// parity: `ClientboundGameEventPacket.PUFFER_FISH_STING` (9).
constexpr std::uint8_t kStingGameEvent = 9;

[[nodiscard]] Aabb boxOf(const ServerEntity& entity) {
    return Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
}

[[nodiscard]] Aabb boxOf(const PlayerSession& player) {
    // parity: `EntityType.PLAYER`'s box, which is what a touch is measured against.
    return Aabb::of(
        player.x, player.y, player.z,
        static_cast<double>(server::registry::entityTypeShapes::widthOf(wire::common::Identifier::vanilla("player"))),
        static_cast<double>(server::registry::entityTypeShapes::heightOf(wire::common::Identifier::vanilla("player"))));
}

/// parity: `Pufferfish.SCARY_MOB`.
[[nodiscard]] bool scary(const ServerEntity& other, const world::realEntityTypeTags::TagMembers* tags) {
    return tags == nullptr ||
           !world::realEntityTypeTags::membersOf(*tags, wire::common::Identifier::vanilla("not_scary_for_pufferfish"))
                .contains(other.type());
}

} // namespace

void MobActionSystem::tickPufferfish(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                     ServerEntity& fish, const std::vector<ServerEntity*>& living) {
    const bool alive = !fish.removed && !fish.dying && fish.health > 0.0;
    // parity: `Pufferfish.tick`, which only runs with effective AI, and `isAlive`.
    if (alive && !fish.noAi) {
        if (fish.puffInflateCounter > 0) {
            if (fish.puffState == 0) {
                makeSound(world, fish, "entity.puffer_fish.blow_up", 1.0F);
                fish.puffState = 1;
            } else if (fish.puffInflateCounter > kFullPuffAt && fish.puffState == 1) {
                makeSound(world, fish, "entity.puffer_fish.blow_up", 1.0F);
                fish.puffState = 2;
            }
            ++fish.puffInflateCounter;
        } else if (fish.puffState != 0) {
            if (fish.puffDeflateTimer > kHalfDeflatedAfter && fish.puffState == 2) {
                makeSound(world, fish, "entity.puffer_fish.blow_out", 1.0F);
                fish.puffState = 1;
            } else if (fish.puffDeflateTimer > kDeflatedAfter && fish.puffState == 1) {
                makeSound(world, fish, "entity.puffer_fish.blow_out", 1.0F);
                fish.puffState = 0;
            }
            ++fish.puffDeflateTimer;
        }
    }
    // parity: the PufferfishPuffGoal's start and stop, which decide whether the tick above counts.
    if (!fish.noAi) {
        const Aabb reach = boxOf(fish).inflate(kScareInflate);
        bool frightened = false;
        for (const ServerEntity* other : living) {
            if (other == &fish || other->removed || !server::registry::entityAttributes::isLiving(other->type()) ||
                !scary(*other, options_.entityTags) || !reach.intersects(boxOf(*other))) {
                continue;
            }
            frightened = true;
            break;
        }
        if (!frightened) {
            for (const PlayerSession* player : watchers) {
                if (player->gamemode == Gamemode::Creative) {
                    continue;
                }
                const Aabb body = boxOf(*player);
                if (reach.intersects(body)) {
                    frightened = true;
                    break;
                }
            }
        }
        if (frightened && fish.puffInflateCounter == 0) {
            fish.puffInflateCounter = 1;
            fish.puffDeflateTimer = 0;
        } else if (!frightened && fish.puffInflateCounter > 0) {
            fish.puffInflateCounter = 0;
        }
    }
    // parity: `Pufferfish.aiStep`'s touch and `playerTouch`, which run however the AI is set.
    if (!alive || fish.puffState == 0) {
        return;
    }
    const Aabb aura = boxOf(fish).inflate(kAuraInflate);
    const auto teamsAllow = [this, &fish](std::string_view member) {
        // parity: `LivingEntity.doTeamsAllowDamage`: no team of its own, or friendly fire on.
        if (options_.teams == nullptr) {
            return true;
        }
        const scoreboard::Team* own = options_.teams->teamOf(fish.uuid().toString());
        return own == nullptr || own != options_.teams->teamOf(member) || own->friendlyFire;
    };
    for (ServerEntity* other : living) {
        // parity: `getEntitiesOfClass(Mob.class, ...)`.
        if (other == &fish || other->removed || other->health <= 0.0 || !entity::mobs::isMob(other->type()) ||
            !scary(*other, options_.entityTags) || !aura.intersects(boxOf(*other))) {
            continue;
        }
        if (!teamsAllow(other->uuid().toString())) {
            continue;
        }
        entity::entityCombat::blame(*other, fish.entityId(), fish.entityId(), fish.type());
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(*other, kStingPerPuff + static_cast<float>(fish.puffState), fish.x, fish.z,
                                       hooks_.combat, "mob_attack", fish.type().toString());
        if (!result.landed) {
            continue;
        }
        // parity: `Pufferfish.touch`, a second of poison per puff level and the sting sound.
        static_cast<void>(addMobEffect(world, watchers, *other,
                                       world::ActiveEffect{.effect = wire::common::Identifier::vanilla("poison"),
                                                           .remainingTicks = kPoisonPerPuff * fish.puffState},
                                       fish.entityId()));
        playSound(world, fish, "entity.puffer_fish.sting");
        combat_->announceEntityHit(watchers, *other, world::damageTypes::kMobAttack, fish.entityId(), result.died);
        world::vibrations::emit(world.state(), "entity_damage", other->x, other->y, other->z, world.gameTime(),
                                {.entity = other->uuid()});
    }
    // parity: `Player.touch`.
    const Aabb grown = boxOf(fish);
    for (PlayerSession* player : watchers) {
        if (player->dead || player->gamemode == Gamemode::Spectator) {
            continue;
        }
        // parity: `AABB.inflate(1.0, 0.5, 1.0)`, the pickup area a player touches entities with.
        const Aabb body = boxOf(*player);
        const Aabb pickup{.minX = body.minX - 1.0,
                          .minY = body.minY - 0.5,
                          .minZ = body.minZ - 1.0,
                          .maxX = body.maxX + 1.0,
                          .maxY = body.maxY + 0.5,
                          .maxZ = body.maxZ + 1.0};
        if (!pickup.intersects(grown)) {
            continue;
        }
        if (!player->connection().hurt) {
            continue;
        }
        const std::int32_t invulnerableBefore = player->invulnerableTicks;
        const float healthBefore = player->health();
        const float lastHurtBefore = player->lastHurt;
        player->connection().hurt(
            player::combat::afterDifficulty(kStingPerPuff + static_cast<float>(fish.puffState), world.difficulty()),
            "mob", world::damageTypes::kMobAttack,
            entity::DamageSource{.entityId = fish.entityId(),
                                 .x = fish.x,
                                 .z = fish.z,
                                 .name = entity::entityCombat::displayName(fish)});
        const bool landed = player->invulnerableTicks != invulnerableBefore || player->health() != healthBefore ||
                            player->lastHurt != lastHurtBefore;
        if (!landed) {
            continue;
        }
        // parity: `Pufferfish.playerTouch`, the game-event flash unless the fish is silent.
        if (!(fish.sourceNbt.has_value() && fish.sourceNbt->getBoolean("Silent")) &&
            player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(
                wire::protocol::play::GameEventPacket::kId, [](wire::io::ByteBuffer& buffer) {
                    wire::protocol::play::GameEventPacket{.event = kStingGameEvent, .param = 0.0F}.write(buffer);
                });
        }
        applyEffect(player->entityId(), "poison", kPoisonPerPuff * fish.puffState, 0, fish.entityId());
    }
}

} // namespace basalt::server::runtime
