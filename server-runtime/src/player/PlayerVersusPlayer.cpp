#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerMace.hpp>
#include <basalt/server/runtime/player/PlayerMovementStats.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PostAttackEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace basalt::server::runtime::player::pvp {
namespace {

using entity::ServerEntity;
using wire::common::Identifier;
using wire::protocol::play::SoundSource;
namespace mth = wire::common::mth;

/// parity: `Player.attack`'s sprinting push, on top of the weapon's.
constexpr float kSprintKnockback = 0.5F;
/// parity: `doSweepAttack`'s own push on everything it caught.
constexpr double kSweepKnockback = 0.4;
constexpr double kSweepReachSquared = 9.0;
constexpr double kSweepInflateSides = 1.0;
constexpr double kSweepInflateHeight = 0.25;
constexpr std::int32_t kPanicTicks = 40;
constexpr float kIndicatorThreshold = 2.0F;

[[nodiscard]] Identifier playerType() { return Identifier::vanilla("player"); }

[[nodiscard]] double playerHeight() { return server::registry::entityTypeShapes::heightOf(playerType()); }

[[nodiscard]] entity::Aabb boxOf(const PlayerSession& player) {
    return entity::Aabb::of(player.x, player.y, player.z, server::registry::entityTypeShapes::widthOf(playerType()),
                            playerHeight());
}

[[nodiscard]] world::weaponDamage::DefinitionOf definitionsOf(const Context& context) {
    world::RealEnchantments* enchantments = context.enchantments;
    return [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
}

[[nodiscard]] world::weaponDamage::Victim victimOf(const Context& context, const Identifier& type) {
    const world::realEntityTypeTags::TagMembers* tags = context.entityTags;
    return world::weaponDamage::Victim{.type = type, .inTag = [tags, type](const Identifier& tag) {
                                           return tags != nullptr &&
                                                  world::realEntityTypeTags::membersOf(*tags, tag).contains(type);
                                       }};
}

[[nodiscard]] std::vector<PlayerSession*> playersIn(const Context& context, std::string_view worldName) {
    std::vector<PlayerSession*> found;
    if (context.registry == nullptr) {
        return found;
    }
    for (PlayerSession* player : context.registry->snapshot()) {
        if (player->worldName == worldName) {
            found.push_back(player);
        }
    }
    return found;
}

/// parity: `playServerSideSound`, at the attacker for everyone near.
void soundAt(const Context& context, const PlayerSession& attacker, std::string_view sound) {
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const std::int64_t seed = random.nextLong();
    for (const PlayerSession* player : playersIn(context, attacker.worldName)) {
        playSoundFor(*player, sound, SoundSource::Players, attacker.x, attacker.y, attacker.z, 1.0F, 1.0F, seed);
    }
}

void animate(const Context& context, std::string_view worldName, std::int32_t entityId, std::uint8_t action) {
    const wire::protocol::play::AnimatePacket packet{.entityId = entityId, .action = action};
    for (PlayerSession* player : playersIn(context, worldName)) {
        if (action == wire::protocol::play::AnimatePacket::kCriticalHit && player->connection().showCriticalHit) {
            player->connection().showCriticalHit(entityId);
        } else if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::AnimatePacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

[[nodiscard]] std::array<double, 2> facing(const PlayerSession& attacker) {
    const float yaw = attacker.yaw * (std::numbers::pi_v<float> / 180.0F);
    return {static_cast<double>(mth::sin(yaw)), static_cast<double>(-mth::cos(yaw))};
}

[[nodiscard]] float angleFrom(const PlayerSession& attacker, const PlayerSession& victim) {
    return static_cast<float>(std::atan2(attacker.z - victim.z, attacker.x - victim.x) * 180.0 / std::numbers::pi) -
           victim.yaw;
}

/// parity: `LivingEntity.knockback` on a player.
void pushPlayer(const Context& context, PlayerSession& attacker, PlayerSession& victim, double power) {
    const std::array<double, 2> towards = facing(attacker);
    const double resistance =
        playerAttributes::valueOf(victim, "minecraft:knockback_resistance", definitionsOf(context));
    sendMotion(victim,
               combat::knockback(power, towards[0], towards[1], victim.deltaMovement[0], victim.deltaMovement[1],
                                 victim.deltaMovement[2], victim.onGround, resistance),
               angleFrom(attacker, victim));
}

[[nodiscard]] std::vector<world::postAttack::Equipped> equipmentOf(const PlayerSession& player) {
    static constexpr std::array<std::pair<std::int32_t, std::string_view>, 4> kWorn{
        {{PlayerInventory::kArmourStart + 3, "feet"},
         {PlayerInventory::kArmourStart + 2, "legs"},
         {PlayerInventory::kArmourStart + 1, "chest"},
         {PlayerInventory::kArmourStart, "head"}}};
    std::vector<world::postAttack::Equipped> equipped;
    const auto add = [&](std::int32_t slot, std::string_view name) {
        if (const std::optional<storage::StoredItemStack> stack = player.inventory.at(slot); stack.has_value()) {
            equipped.push_back({.slot = slot, .equipmentSlot = name, .stack = *stack});
        }
    };
    add(PlayerInventory::kHotbarStart + player.inventory.heldSlot(), "mainhand");
    add(PlayerInventory::kOffhandSlot, "offhand");
    for (const auto& [slot, name] : kWorn) {
        add(slot, name);
    }
    return equipped;
}

void wearSlot(const Context& context, PlayerSession& owner, std::int32_t slot, std::int32_t amount) {
    if (context.blocks == nullptr || amount <= 0) {
        return;
    }
    BreakContext blocks = *context.blocks;
    blocks.slotChanged = [](const PlayerSession& player, std::int32_t) {
        if (player.connection().sendInventoryAll) {
            player.connection().sendInventoryAll();
        }
    };
    wearItemIn(blocks, owner, slot, amount);
}

/// Whether the attacker's team is also theirs. parity: `Entity.isAlliedTo`.
[[nodiscard]] bool allied(const Context& context, const PlayerSession& attacker, std::string_view member,
                          std::int32_t otherId) {
    const scoreboard::Team* own = context.teams == nullptr ? nullptr : context.teams->teamOf(attacker.name());
    return (own != nullptr && own == context.teams->teamOf(member)) || alliedByRules(attacker, otherId);
}

[[nodiscard]] double distanceSquared(const PlayerSession& attacker, double x, double y, double z) {
    const double dx = x - attacker.x;
    const double dy = y - attacker.y;
    const double dz = z - attacker.z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

/// parity: `itemAttackInteraction`'s `hurtEnemy`: a `weapon` counts as used, and wears.
void wearWeapon(const Context& context, PlayerSession& attacker, const std::optional<storage::StoredItemStack>& weapon,
                std::int32_t slot) {
    if (!weapon.has_value() || item::itemWear::costPerAttack(*weapon) <= 0) {
        return;
    }
    statAwards::used(attacker, weapon->id);
    if (context.blocks != nullptr) {
        wearItemIn(*context.blocks, attacker, slot, item::itemWear::costPerAttack(*weapon));
    }
}

[[nodiscard]] bool isSword(const Context& context, const std::optional<storage::StoredItemStack>& weapon) {
    if (!weapon.has_value()) {
        return false;
    }
    const auto id = Identifier::parse(weapon->id);
    if (!id.has_value()) {
        return false;
    }
    if (context.blocks != nullptr && context.blocks->itemTags != nullptr) {
        return world::realItemTags::contains(*context.blocks->itemTags, "minecraft:swords", *id);
    }
    return id->path().ends_with("_sword");
}

} // namespace

bool alliedByRules(const PlayerSession& attacker, const std::int32_t otherId) {
    if (attacker.events == nullptr) {
        return false;
    }
    const server::plugins::PluginEvent* answered = attacker.events->ask(
        &server::plugins::pluginEventTypes::entityAlliedCheck, [&](server::plugins::PluginEvent& event) {
            plugins::hookEvents::entity(event, attacker.worldName, attacker.entityId(), "minecraft:player", attacker.x,
                                        attacker.y, attacker.z);
            event.set("other_id", static_cast<std::int64_t>(otherId));
            event.set("allied", false);
        });
    const bool* allied = answered == nullptr ? nullptr : std::get_if<bool>(&answered->get("allied"));
    return allied != nullptr && *allied;
}

/// parity: `EnchantmentHelper.doPostAttackEffectsWithItemSource` after a melee blow on `victim` or `mob`.
void postAttack(const Context& context, world::LoadedWorld& world, PlayerSession& attacker, PlayerSession* victim,
                ServerEntity* mob, const std::optional<storage::StoredItemStack>& weapon, std::int32_t weaponSlot) {
    const world::weaponDamage::Victim facts = victimOf(context, victim != nullptr ? playerType() : mob->type());
    world::postAttack::Hit hit{
        .victim = &facts, .attackerFallDistance = attacker.fallDistance, .attackerFlying = attacker.fallFlying};
    if (victim != nullptr) {
        hit.victimEquipment = equipmentOf(*victim);
    }
    if (weapon.has_value()) {
        hit.weapon = world::postAttack::Equipped{.slot = weaponSlot, .equipmentSlot = "mainhand", .stack = *weapon};
    }
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const world::postAttack::Outcome outcome = world::postAttack::of(hit, definitionsOf(context), random);
    using world::postAttack::Side;
    for (const world::postAttack::Ignite& ignite : outcome.ignites) {
        // parity: `igniteForTicks` only ever lengthens the fire.
        if (ignite.target == Side::kAttacker) {
            attacker.fireTicks = std::max(attacker.fireTicks, ignite.ticks);
        } else if (victim != nullptr) {
            victim->fireTicks = std::max(victim->fireTicks, ignite.ticks);
        } else {
            mob->fireTicks = std::max(mob->fireTicks, ignite.ticks);
        }
    }
    for (const world::postAttack::Damage& damage : outcome.damages) {
        // parity: `DamageEntity`'s source is whoever owns the enchanted item.
        PlayerSession& owner = damage.target == Side::kAttacker && victim != nullptr ? *victim : attacker;
        PlayerSession* target = damage.target == Side::kAttacker ? &attacker : victim;
        if (target != nullptr && target != &owner && target->connection().hurt) {
            target->connection().hurt(damage.amount, damage.type.path(), damage.type,
                                      entity::DamageSource{.entityId = owner.entityId(),
                                                           .x = owner.x,
                                                           .z = owner.z,
                                                           .name = wire::text::Component::text(owner.name())});
        } else if (target == nullptr && mob != nullptr) {
            static_cast<void>(entity::entityCombat::hurt(*mob, damage.amount, owner.x, owner.z, {},
                                                         std::string{damage.type.path()}, std::string{owner.name()}));
        }
    }
    for (const world::postAttack::MobEffect& effect : outcome.effects) {
        PlayerSession* target = effect.target == Side::kAttacker ? &attacker : victim;
        if (target == nullptr && mob != nullptr && context.addMobEffect) {
            context.addMobEffect(world, *mob,
                                 world::ActiveEffect{.effect = effect.effect,
                                                     .amplifier = effect.amplifier,
                                                     .remainingTicks = effect.ticks},
                                 attacker.entityId());
        } else if (target != nullptr && target->connection().addEffect) {
            target->noteEffectSource(std::nullopt);
            target->connection().addEffect(effect.effect, effect.ticks, effect.amplifier);
        }
    }
    for (const world::postAttack::ItemWear& wear : outcome.wear) {
        PlayerSession* owner = wear.owner == Side::kAttacker ? &attacker : victim;
        if (owner != nullptr) {
            wearSlot(context, *owner, wear.slot, wear.amount);
        }
    }
    for (const world::postAttack::Explosion& blast : outcome.explosions) {
        if (!context.explode) {
            break;
        }
        if (blast.target == Side::kAttacker) {
            context.explode(world, blast, attacker.x, attacker.y, attacker.z);
        } else if (victim != nullptr) {
            context.explode(world, blast, victim->x, victim->y, victim->z);
        } else {
            context.explode(world, blast, mob->x, mob->y, mob->z);
        }
    }
}

void itemAttackInteraction(const Context& context, world::LoadedWorld& world, PlayerSession& attacker,
                           PlayerSession* victim, ServerEntity* mob,
                           const std::optional<storage::StoredItemStack>& weapon, std::int32_t weaponSlot) {
    const bool smash = mace::smashes(attacker, weapon);
    if (smash) {
        const mace::Struck struck = victim != nullptr ? mace::Struck{.entityId = victim->entityId(),
                                                                     .x = victim->x,
                                                                     .y = victim->y,
                                                                     .z = victim->z,
                                                                     .onGround = victim->onGround,
                                                                     .uuid = victim->uuid().toString()}
                                                      : mace::Struck{.entityId = mob->entityId(),
                                                                     .x = mob->x,
                                                                     .y = mob->y,
                                                                     .z = mob->z,
                                                                     .onGround = mob->onGround,
                                                                     .uuid = mob->uuid().toString()};
        mace::hurtEnemy(context, world, attacker, struck);
    }
    postAttack(context, world, attacker, victim, mob, weapon, weaponSlot);
    if (smash) {
        mace::postHurtEnemy(attacker);
    }
    wearWeapon(context, attacker, weapon, weaponSlot);
}

float knockbackOf(const Context& context, const PlayerSession& attacker,
                  const std::optional<storage::StoredItemStack>& weapon) {
    const world::weaponDamage::DefinitionOf definitionOf = definitionsOf(context);
    return (static_cast<float>(playerAttributes::valueOf(attacker, "minecraft:attack_knockback", definitionOf)) +
            (weapon.has_value() ? world::weaponDamage::valueEffect(*weapon, "minecraft:knockback", 0.0F, definitionOf)
                                : 0.0F)) /
           2.0F;
}

void knockMob(PlayerSession& attacker, ServerEntity& mob, float power) {
    if (power <= 0.0F) {
        return;
    }
    const std::array<double, 2> towards = facing(attacker);
    entity::entityCombat::knockback(mob, static_cast<double>(power), towards[0], towards[1]);
    attacker.sprinting = attacker.sprinting && attacker.keepSprintOnAttack;
}

bool criticalPossible(const PlayerSession& attacker, world::LoadedWorld& world, bool falling) {
    if (!falling || attacker.onGround || attacker.vehicleId.has_value() ||
        attacker.effects.contains(Identifier::vanilla("blindness"))) {
        return false;
    }
    const std::optional<std::int32_t> feet = world.state().blockState(
        static_cast<std::int32_t>(std::floor(attacker.x)), static_cast<std::int32_t>(std::floor(attacker.y)),
        static_cast<std::int32_t>(std::floor(attacker.z)));
    return !feet.has_value() ||
           (!playerMovement::isClimbable(*feet) && world::fluids::of(*feet) != world::Submersion::Water);
}

bool canHarm(const PlayerSession& victim, const PlayerSession& attacker, bool pvpAllowed,
             const scoreboard::Teams* teams) {
    if (!pvpAllowed) {
        return false;
    }
    const scoreboard::Team* team = teams == nullptr ? nullptr : teams->teamOf(victim.name());
    if (team == nullptr) {
        return true;
    }
    // parity: `PlayerTeam.isAlliedTo` is the same team and nothing else.
    return team != teams->teamOf(attacker.name()) || team->friendlyFire;
}

void sendMotion(PlayerSession& player, const std::array<double, 3>& motion, float fromYaw) {
    if (player.connection().pushBack) {
        player.connection().pushBack(motion[0], motion[1], motion[2], fromYaw);
        return;
    }
    if (!player.connection().sendPlayPacket) {
        return;
    }
    player.connection().sendPlayPacket(
        wire::protocol::play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::SetEntityMotionPacket{
                .entityId = player.entityId(), .velocityX = motion[0], .velocityY = motion[1], .velocityZ = motion[2]}
                .write(buffer);
        });
}

void showDamageIndicator(const std::vector<PlayerSession*>& players, double x, double y, double z, float lost) {
    if (lost <= kIndicatorThreshold) {
        return;
    }
    const server::registry::ParticleType* type =
        server::registry::particleTypes::of(Identifier::vanilla("damage_indicator"));
    if (type == nullptr) {
        return;
    }
    wire::protocol::play::LevelParticlesPacket packet{.particleTypeId = type->protocolId,
                                                      .x = x,
                                                      .y = y,
                                                      .z = z,
                                                      .offsetX = 0.1F,
                                                      .offsetZ = 0.1F,
                                                      .count =
                                                          static_cast<std::int32_t>(static_cast<double>(lost) * 0.5)};
    packet.setMaxSpeed(0.2F);
    for (const PlayerSession* player : players) {
        showParticleFor(*player, type->id.toString(), packet);
    }
}

bool attack(PlayerSession& attacker, PlayerSession& victim, world::LoadedWorld& world, const Context& context,
            const Swing& swing) {
    const world::weaponDamage::DefinitionOf definitionOf = definitionsOf(context);
    const world::weaponDamage::Victim facts = victimOf(context, playerType());
    const float enchanted = world::weaponDamage::of(swing.weapon, swing.base, definitionOf, &facts);
    const float magic = swing.strength * (enchanted - swing.base);
    // parity: the charged base or the enchantments have to be worth something before any bonus.
    if (swing.base * combat::chargeScale(swing.strength) <= 0.0F && magic <= 0.0F) {
        return false;
    }
    const bool smash = mace::smashes(attacker, swing.weapon);
    const float bonus = mace::damageBonus(attacker, swing.weapon, definitionOf);
    const combat::Swing blow =
        combat::swingOf(swing.strength, swing.base, enchanted, swing.falling, attacker.onGround, attacker.sprinting,
                        bonus, world.state().gameplay().player.criticalDamageMultiplier);
    if (blow.knockbackAttack) {
        soundAt(context, attacker, "entity.player.attack.knockback");
    }
    const bool sweeping = blow.fullStrength && !blow.critical && !blow.knockbackAttack && attacker.onGround &&
                          swing.slowEnoughToSweep && isSword(context, swing.weapon);
    const float before = victim.health();
    const std::array<double, 3> oldMovement = victim.deltaMovement;
    victim.lastHurtLanded = false;
    if (victim.connection().hurt) {
        // parity: `MaceItem.getItemDamageSource`, `mace_smash` for a smash.
        victim.connection().hurt(
            blow.damage, smash ? "mace_smash" : "player",
            smash ? world::damageTypes::kMaceSmash : world::damageTypes::kPlayerAttack,
            entity::DamageSource{.entityId = attacker.entityId(),
                                 .x = attacker.x,
                                 .z = attacker.z,
                                 .name = wire::text::Component::text(attacker.name()),
                                 .ownerId = attacker.entityId(),
                                 .armorFractionAdd = mace::armorFractionAdd(swing.weapon, definitionOf)});
    }
    if (!victim.lastHurtLanded) {
        soundAt(context, attacker, "entity.player.attack.nodamage");
        return false;
    }
    // parity: `causeExtraKnockback`.
    const float knockback =
        knockbackOf(context, attacker, swing.weapon) + (blow.knockbackAttack ? kSprintKnockback : 0.0F);
    if (knockback > 0.0F) {
        pushPlayer(context, attacker, victim, static_cast<double>(knockback));
        attacker.sprinting = attacker.sprinting && attacker.keepSprintOnAttack;
    }
    victim.deltaMovement = oldMovement;
    if (sweeping) {
        sweep(attacker, world, context, victim.entityId(), boxOf(victim),
              (swing.base * combat::chargeScale(swing.strength)) + bonus, swing.strength, swing.weapon);
    }
    // parity: `attackVisualEffects`.
    if (blow.critical) {
        soundAt(context, attacker, "entity.player.attack.crit");
        animate(context, attacker.worldName, victim.entityId(), wire::protocol::play::AnimatePacket::kCriticalHit);
    }
    if (!blow.critical && !sweeping) {
        soundAt(context, attacker, blow.fullStrength ? "entity.player.attack.strong" : "entity.player.attack.weak");
    }
    if (magic > 0.0F) {
        animate(context, attacker.worldName, victim.entityId(), wire::protocol::play::AnimatePacket::kMagicCriticalHit);
    }
    // parity: `setLastHurtMob`, which `OwnerHurtTargetGoal` reads.
    for (ServerEntity* pet : world.state().entities()) {
        if (pet->owner.has_value() && *pet->owner == attacker.uuid().toString()) {
            pet->ownerHurt = victim.entityId();
        }
    }
    itemAttackInteraction(context, world, attacker, &victim, nullptr, swing.weapon, swing.weaponSlot);
    const float lost = before - std::max(victim.health(), 0.0F);
    attacker.stats.addDamageDealt(lost);
    showDamageIndicator(playersIn(context, attacker.worldName), victim.x, victim.y + (playerHeight() * 0.5), victim.z,
                        lost);
    attacker.food.addExhaustion(attacker.food.tuning.combat);
    return true;
}

void sweep(PlayerSession& attacker, world::LoadedWorld& world, const Context& context, std::int32_t targetId,
           const entity::Aabb& targetBox, float baseDamage, float strength,
           const std::optional<storage::StoredItemStack>& weapon) {
    soundAt(context, attacker, "entity.player.attack.sweep");
    const world::weaponDamage::DefinitionOf definitionOf = definitionsOf(context);
    const float sweepDamage =
        1.0F +
        (static_cast<float>(playerAttributes::valueOf(attacker, "minecraft:sweeping_damage_ratio", definitionOf)) *
         baseDamage);
    const entity::Aabb reach{.minX = targetBox.minX - kSweepInflateSides,
                             .minY = targetBox.minY - kSweepInflateHeight,
                             .minZ = targetBox.minZ - kSweepInflateSides,
                             .maxX = targetBox.maxX + kSweepInflateSides,
                             .maxY = targetBox.maxY + kSweepInflateHeight,
                             .maxZ = targetBox.maxZ + kSweepInflateSides};
    const std::array<double, 2> towards = facing(attacker);
    const std::int32_t weaponSlot = PlayerInventory::kHotbarStart + attacker.inventory.heldSlot();
    for (ServerEntity* nearby : world.state().entities()) {
        if (nearby->entityId() == targetId || nearby->removed || nearby->dying ||
            !server::registry::entityAttributes::isLiving(nearby->type()) ||
            !entity::Aabb::of(nearby->x, nearby->y, nearby->z, nearby->width(), nearby->height()).intersects(reach) ||
            allied(context, attacker, nearby->uuid().toString(), nearby->entityId()) ||
            (nearby->type().path() == "armor_stand" && nearby->sourceNbt.has_value() &&
             nearby->sourceNbt->getBoolean("Marker")) ||
            distanceSquared(attacker, nearby->x, nearby->y, nearby->z) >= kSweepReachSquared) {
            continue;
        }
        const world::weaponDamage::Victim facts = victimOf(context, nearby->type());
        const float damage = world::weaponDamage::of(weapon, sweepDamage, definitionOf, &facts) * strength;
        entity::entityCombat::blame(*nearby, attacker.entityId(), attacker.entityId(), playerType());
        const entity::entityCombat::Result result = entity::entityCombat::hurt(
            *nearby, damage, attacker.x, attacker.z, {}, "attack", std::string{attacker.name()}, true);
        if (!result.landed) {
            continue;
        }
        entity::entityCombat::knockback(*nearby, kSweepKnockback, towards[0], towards[1]);
        entity::entityCombat::rememberPlayer(*nearby, attacker.uuid());
        world::vibrations::emit(world.state(), "entity_damage", nearby->x, nearby->y, nearby->z, world.gameTime(),
                                {.entity = nearby->uuid()});
        if (!result.died) {
            nearby->hurtBy = attacker.entityId();
            if (entity::wardens::isWarden(nearby->type())) {
                entity::wardens::anger(*nearby, attacker.entityId(), entity::wardens::kHurt);
                MobActionSystem::wardenStruck(*nearby, attacker.entityId(), true, 0.0);
            }
            nearby->panicTicks = kPanicTicks;
            postAttack(context, world, attacker, nullptr, nearby, weapon, weaponSlot);
        }
        if (context.sweptMob) {
            context.sweptMob(*nearby, result.died);
        }
    }
    for (PlayerSession* nearby : playersIn(context, attacker.worldName)) {
        if (nearby == &attacker || nearby->entityId() == targetId || nearby->dead ||
            nearby->gamemode == wire::protocol::play::Gamemode::Spectator || !boxOf(*nearby).intersects(reach) ||
            allied(context, attacker, nearby->name(), nearby->entityId()) ||
            distanceSquared(attacker, nearby->x, nearby->y, nearby->z) >= kSweepReachSquared ||
            !nearby->connection().hurt) {
            continue;
        }
        const world::weaponDamage::Victim facts = victimOf(context, playerType());
        const float damage = world::weaponDamage::of(weapon, sweepDamage, definitionOf, &facts) * strength;
        const std::array<double, 3> oldMovement = nearby->deltaMovement;
        nearby->lastHurtLanded = false;
        nearby->connection().hurt(damage, "player", world::damageTypes::kPlayerAttack,
                                  entity::DamageSource{.entityId = attacker.entityId(),
                                                       .x = attacker.x,
                                                       .z = attacker.z,
                                                       .name = wire::text::Component::text(attacker.name()),
                                                       .ownerId = attacker.entityId()});
        if (!nearby->lastHurtLanded) {
            continue;
        }
        pushPlayer(context, attacker, *nearby, kSweepKnockback);
        nearby->deltaMovement = oldMovement;
        postAttack(context, world, attacker, nearby, nullptr, weapon, weaponSlot);
    }
    // parity: the sweep's particle a block ahead of the attacker, at half their height.
    const server::registry::ParticleType* type =
        server::registry::particleTypes::of(Identifier::vanilla("sweep_attack"));
    if (type != nullptr) {
        const wire::protocol::play::LevelParticlesPacket packet{.particleTypeId = type->protocolId,
                                                                .x = attacker.x - towards[0],
                                                                .y = attacker.y + (playerHeight() * 0.5),
                                                                .z = attacker.z - towards[1],
                                                                .count = 0};
        for (const PlayerSession* player : playersIn(context, attacker.worldName)) {
            showParticleFor(*player, type->id.toString(), packet);
        }
    }
}

} // namespace basalt::server::runtime::player::pvp
