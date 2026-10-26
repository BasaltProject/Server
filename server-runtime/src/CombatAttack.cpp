#include "CombatAttackShared.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VehicleSystem.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/DragonState.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerMace.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerDrops.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

namespace basalt::server::runtime {
using namespace combatAttack;

void CombatHandler::handleAttack(const wire::protocol::play::AttackPacket& attack) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || session->dead || session->gamemode == Gamemode::Spectator || world == nullptr) {
        return;
    }
    ServerEntity* target = world->state().entity(attack.entityId);
    std::optional<entity::enderDragon::Part> dragonPart;
    if (target == nullptr) {
        // parity: `EnderDragonPart`, an entity of its own that hands every hit to the dragon.
        for (ServerEntity* candidate : world->state().entities()) {
            if (candidate->dragon == nullptr) {
                continue;
            }
            dragonPart = entity::enderDragon::partByEntityId(candidate->entityId(), attack.entityId);
            if (dragonPart.has_value()) {
                target = candidate;
                break;
            }
        }
    }
    // parity: `Player.attack` on a `ServerPlayer`, who is no entity of the world's.
    if (target == nullptr) {
        for (PlayerSession* other : owner_->playersInSameWorld()) {
            if (other->entityId() != attack.entityId || other == session) {
                continue;
            }
            const Identifier playerType = Identifier::vanilla("player");
            if (player::combat::withinAttackReach(session->x, session->y, session->z, other->x, other->y, other->z,
                                                  server::registry::entityTypeShapes::widthOf(playerType),
                                                  server::registry::entityTypeShapes::heightOf(playerType)) &&
                (!hooks_.attackAllowed || hooks_.attackAllowed(*session, other->entityId()))) {
                attackPlayer(*session, *other, *world);
            }
            return;
        }
    }
    if (target == nullptr || target->removed) {
        return;
    }
    if (dragonPart.has_value()) {
        const entity::enderDragon::Vec3& at = target->dragon->parts[static_cast<std::size_t>(*dragonPart)];
        const entity::enderDragon::PartBox& box =
            entity::enderDragon::kPartBoxes[static_cast<std::size_t>(*dragonPart)];
        if (!player::combat::withinAttackReach(session->x, session->y, session->z, at.x, at.y, at.z,
                                               static_cast<double>(box.width), static_cast<double>(box.height))) {
            return;
        }
    } else if (!spinning_ && !owner_->withinAttackReach(*session, *target)) {
        return;
    }
    if (hooks_.attackAllowed && !hooks_.attackAllowed(*session, target->entityId())) {
        return;
    }
    if (world::detectorRails::isMinecart(target->type()) || entity::boats::isBoat(target->type())) {
        breakVehicle(*session, *target);
        return;
    }
    // parity: `ItemFrame.hurtServer`.
    // frame or takes it off the wall.
    if (world::itemFrames::isFrame(target->type())) {
        breakFrame(*session, *target);
        return;
    }
    if (world::cushions::isCushion(target->type())) {
        breakCushion(*session, *target);
        return;
    }
    // parity: `BlockAttachedEntity.hurtServer` on a knot.
    if (target->type().path() == "leash_knot") {
        target->removed = true;
        owner_->broadcastSound("minecraft:entity.lead.untied", target->x, target->y, target->z, SoundSource::Neutral);
        return;
    }
    if (world::endCrystals::isCrystal(target->type())) {
        // parity: `EndCrystal.hurtServer`: any blow that counts destroys it and sets it off.
        if (owner_->options().combat != nullptr) {
            owner_->options().combat->hitCrystal(*world, owner_->playersInSameWorld(), *target, false, false,
                                                 session->entityId(), session->gamemode == Gamemode::Creative);
        }
        return;
    }
    // parity: `ShulkerBullet.hurtServer`.
    if (entity::shulkers::isBullet(target->type())) {
        session->ticksSinceSwing = 0;
        owner_->broadcastSound("entity.shulker_bullet.hurt", target->x, target->y, target->z, SoundSource::Hostile);
        broadcastParticle("minecraft:crit", target->x, target->y, target->z, 15);
        target->removed = true;
        world::vibrations::emit(world->state(), "entity_damage", target->x, target->y, target->z, world->gameTime(),
                                {.entity = target->uuid()});
        return;
    }
    // parity: `Player.attack` turns a redirectable projectile back instead of hurting it.
    if (CombatSystem::isRedirectable(target->type())) {
        session->ticksSinceSwing = 0;
        CombatSystem::deflectByPunch(owner_->playersInSameWorld(), *target, *session);
        owner_->broadcastSound(kSoundAttackNoDamage, session->x, session->y, session->z, SoundSource::Players);
        return;
    }
    if (target->type().path() == "interaction") {
        recordInteraction(*target, "attack", *session, world->gameTime());
        return;
    }
    if (!server::registry::entityAttributes::isLiving(target->type())) {
        return;
    }

    // parity: `Player.getWeaponItem` is the riptide's trident while it spins.
    const std::optional<storage::StoredItemStack>& heldStack =
        spinning_ && session->autoSpinItem.has_value() ? session->autoSpinItem : session->inventory.heldItem();
    const std::optional<Identifier> weapon = heldStack.has_value() ? parsedOrNothing(heldStack->id) : std::nullopt;
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const player::playerAttributes::DefinitionOf definitionOf = [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
    // parity: `Player.attack` reads the attack damage and speed attributes.
    const float strength = player::combat::attackStrengthScale(
        session->ticksSinceSwing, player::playerAttributes::valueOf(*session, "minecraft:attack_speed", definitionOf));
    session->ticksSinceSwing = 0;

    // parity: `Player.attack`, the spin's own damage instead of the attribute while spinning.
    const auto base =
        spinning_
            ? session->autoSpinDamage
            : static_cast<float>(player::playerAttributes::valueOf(*session, "minecraft:attack_damage", definitionOf));
    // parity: what the enchantments add rides on the swing but not on the critical, and is put back.
    // on top after the charge and the critical have had the base to themselves.
    const world::realEntityTypeTags::TagMembers* entityTags = owner_->options().entityTypeTags;
    const world::weaponDamage::Victim victim{
        .type = target->type(), .inTag = [entityTags, target](const Identifier& tag) {
            return entityTags != nullptr &&
                   world::realEntityTypeTags::membersOf(*entityTags, tag).contains(target->type());
        }};
    const float enchanted = world::weaponDamage::of(heldStack, base, definitionOf, &victim);
    // parity: `MaceItem.getAttackDamageBonus` and its `mace_smash` source.
    const bool smash = player::mace::smashes(*session, heldStack);
    const float bonus = player::mace::damageBonus(*session, heldStack, definitionOf);
    const player::combat::Swing swing = player::combat::swingOf(
        strength, base, enchanted, player::pvp::criticalPossible(*session, *world, fallDistance > 0.0),
        session->onGround, session->sprinting, bonus, world->state().gameplay().player.criticalDamageMultiplier);
    const Identifier& damageType = smash ? world::damageTypes::kMaceSmash : world::damageTypes::kPlayerAttack;
    const float damage = swing.damage;
    const bool fullStrength = swing.fullStrength;
    const bool critical = swing.critical;
    if (damage <= 0.0F) {
        return;
    }

    const bool knockbackAttack = swing.knockbackAttack;
    if (knockbackAttack) {
        owner_->broadcastSound(kSoundAttackKnockback, session->x, session->y, session->z, SoundSource::Players);
    }
    if (target->dragon != nullptr) {
        target->dragon->hitPart = dragonPart;
    }
    entity::entityCombat::blame(*target, session->entityId(), session->entityId(), Identifier::vanilla("player"));
    const entity::entityCombat::Result result =
        entity::entityCombat::hurt(*target, damage, session->x, session->z, {}, smash ? "mace_smash" : "attack",
                                   session->name(), true, player::mace::armorFractionAdd(heldStack, definitionOf));
    if (!result.landed) {
        owner_->broadcastSound(kSoundAttackNoDamage, session->x, session->y, session->z, SoundSource::Players);
        return;
    }
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        world::triggerFacts::playerHurtEntity(*triggers, *world, *session, *target, damage, damageType);
    }
    // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` at whoever took the hit.
    world::vibrations::emit(world->state(), "entity_damage", target->x, target->y, target->z, world->gameTime(),
                            {.entity = target->uuid()});
    const player::BreakContext blocks = owner_->blockInteraction().breakContext();
    const player::pvp::Context melee = meleeContext(blocks);
    // parity: `causeExtraKnockback`.
    // a sprinting hit adds, along the attacker's facing; the 0.4 every hit is worth was inside it.
    player::pvp::knockMob(*session, *target,
                          player::pvp::knockbackOf(melee, *session, heldStack) +
                              (knockbackAttack ? static_cast<float>(kSprintKnockback) : 0.0F));
    // parity: `isSweepAttack`: a sword, on the ground, walking, and a swing that is neither a.
    // critical nor a sprinting one.
    const bool sweep = fullStrength && !critical && !knockbackAttack && session->onGround &&
                       owner_->lastStepSquared < kSweepSpeedLimit * kSweepSpeedLimit && weapon.has_value() &&
                       weapon->path().ends_with(kSwordSuffix);
    if (sweep) {
        sweepAround(*session, *target, *world, strength, (base * player::combat::chargeScale(strength)) + bonus,
                    heldStack);
    }
    broadcastDamageEvent(*target, damageType, session->entityId());
    broadcastMotion(*target);
    // parity: `HurtByTargetGoal` and `PanicGoal` both read the last thing that hurt it.
    // monster after whoever swung, the other sends an animal running.
    target->hurtBy = session->entityId();
    entity::entityCombat::rememberPlayer(*target, session->uuid());
    // parity: `Warden.hurtServer`.
    if (entity::wardens::isWarden(target->type())) {
        entity::wardens::anger(*target, session->entityId(), entity::wardens::kHurt);
        MobActionSystem::wardenStruck(*target, session->entityId(), true, 0.0);
    }
    target->panicTicks = kPanicTicks;
    // parity: `OwnerHurtTargetGoal`, which sends a pet after whatever its owner just hit.
    for (ServerEntity* pet : world->state().entities()) {
        if (pet->owner.has_value() && *pet->owner == session->uuid().toString()) {
            pet->ownerHurt = target->entityId();
        }
    }
    if (critical) {
        owner_->broadcastSound(kSoundAttackCrit, session->x, session->y, session->z, SoundSource::Players);
        // parity: `ServerPlayer.crit`, which is an animation on the target rather than particles.
        broadcastAnimation(target->entityId(), wire::protocol::play::AnimatePacket::kCriticalHit);
    }
    if (!critical && !sweep) {
        owner_->broadcastSound(fullStrength ? kSoundAttackStrong : kSoundAttackWeak, session->x, session->y, session->z,
                               SoundSource::Players);
    }
    session->stats.addDamageDealt(result.damage);
    // parity: `itemAttackInteraction`.
    player::pvp::itemAttackInteraction(melee, *world, *session, nullptr, target, heldStack,
                                       PlayerInventory::kHotbarStart + session->inventory.heldSlot());
    session->food.addExhaustion(session->food.tuning.combat);
    if (result.died) {
        killEntity(*target);
    } else {
        owner_->broadcastSound(entity::entityCombat::hurtSound(*target), target->x, target->y, target->z,
                               SoundSource::Hostile);
    }
}

void CombatHandler::sweepAround(PlayerSession& session, const ServerEntity& target, world::LoadedWorld& world,
                                float strength, float baseDamage,
                                const std::optional<storage::StoredItemStack>& weapon) {
    const player::BreakContext blocks = owner_->blockInteraction().breakContext();
    player::pvp::sweep(session, world, meleeContext(blocks), target.entityId(),
                       entity::Aabb::of(target.x, target.y, target.z, target.width(), target.height()), baseDamage,
                       strength, weapon);
}

player::pvp::Context CombatHandler::meleeContext(const player::BreakContext& blocks) {
    const ClientSession::Options& options = owner_->options();
    return player::pvp::Context{
        .registry = options.playerRegistry,
        .enchantments = options.enchantments,
        .entityTags = options.entityTypeTags,
        .teams = options.commands == nullptr ? nullptr : options.commands->environment().options().teams,
        .blocks = &blocks,
        .sweptMob =
            [this](ServerEntity& mob, bool died) {
                const PlayerSession* session = owner_->playerSession();
                broadcastDamageEvent(mob, world::damageTypes::kPlayerAttack,
                                     session == nullptr ? -1 : session->entityId());
                broadcastMotion(mob);
                if (died) {
                    killEntity(mob);
                }
            },
        .nextSeed = [] { return static_cast<std::int64_t>(std::random_device{}()); },
        .addMobEffect =
            [this](world::LoadedWorld& world, entity::ServerEntity& mob, const world::ActiveEffect& effect,
                   std::int32_t sourceId) {
                if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr) {
                    static_cast<void>(mobs->addMobEffect(world, owner_->playersInSameWorld(), mob, effect, sourceId));
                }
            },
        .explode =
            [this](world::LoadedWorld& world, const world::postAttack::Explosion& blast, double x, double y, double z) {
                if (CombatSystem* combat = owner_->options().combat; combat != nullptr) {
                    combat->gust(world, owner_->playersInSameWorld(), world.state().entities(),
                                 CombatSystem::gustOf(blast, x, y, z));
                }
            }};
}

void CombatHandler::attackPlayer(PlayerSession& session, PlayerSession& victim, world::LoadedWorld& world) {
    if (victim.dead || victim.gamemode == Gamemode::Spectator) {
        return;
    }
    // parity: `Player.getWeaponItem` is the riptide's trident while it spins.
    const std::optional<storage::StoredItemStack> weapon =
        spinning_ && session.autoSpinItem.has_value() ? session.autoSpinItem : session.inventory.heldItem();
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const player::playerAttributes::DefinitionOf definitionOf = [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
    const float strength = player::combat::attackStrengthScale(
        session.ticksSinceSwing, player::playerAttributes::valueOf(session, "minecraft:attack_speed", definitionOf));
    session.ticksSinceSwing = 0;
    const auto base =
        spinning_
            ? session.autoSpinDamage
            : static_cast<float>(player::playerAttributes::valueOf(session, "minecraft:attack_damage", definitionOf));
    const player::BreakContext blocks = owner_->blockInteraction().breakContext();
    static_cast<void>(player::pvp::attack(
        session, victim, world, meleeContext(blocks),
        player::pvp::Swing{.strength = strength,
                           .base = base,
                           .falling = player::pvp::criticalPossible(session, world, fallDistance > 0.0),
                           .weapon = weapon,
                           .weaponSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot(),
                           .slowEnoughToSweep = owner_->lastStepSquared < kSweepSpeedLimit * kSweepSpeedLimit}));
}

} // namespace basalt::server::runtime
