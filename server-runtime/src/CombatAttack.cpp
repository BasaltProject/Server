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
namespace {

using entity::ServerEntity;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SoundSource;

/// parity: `VehicleEntity.hurtServer`: a cart takes ten a point and comes apart past forty.

constexpr double kSprintKnockback = 0.5;
constexpr double kSweepSpeedLimit = 0.003;
constexpr std::string_view kSwordSuffix = "_sword";

constexpr std::string_view kSoundAttackKnockback = "entity.player.attack.knockback";
constexpr std::string_view kSoundAttackNoDamage = "entity.player.attack.nodamage";
constexpr std::string_view kSoundAttackCrit = "entity.player.attack.crit";
constexpr std::string_view kSoundAttackStrong = "entity.player.attack.strong";
constexpr std::string_view kSoundAttackWeak = "entity.player.attack.weak";

constexpr float kAttackExhaustion = 0.1F;
constexpr std::int32_t kPanicTicks = 40;

/// parity: `ItemEntity`'s default pickup delay of forty ticks.
constexpr std::int32_t kDefaultPickupDelay = 40;

std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

} // namespace

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
    // parity: `ItemFrame.hurtServer`, which is not a hit point of damage: it either empties the
    // frame or takes it off the wall.
    if (world::itemFrames::isFrame(target->type())) {
        breakFrame(*session, *target);
        return;
    }
    if (world::cushions::isCushion(target->type())) {
        breakCushion(*session, *target);
        return;
    }
    // parity: `BlockAttachedEntity.hurtServer` on a knot: it goes, untied, and its mobs lose the lead.
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
    // parity: `ShulkerBullet.hurtServer`: any blow breaks it, with its sound and a burst of crits.
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
    // parity: `Player.attack` reads the attack damage and speed attributes, weapon and effects in.
    const float strength = player::combat::attackStrengthScale(
        session->ticksSinceSwing, player::playerAttributes::valueOf(*session, "minecraft:attack_speed", definitionOf));
    session->ticksSinceSwing = 0;

    // parity: `Player.attack`, the spin's own damage instead of the attribute while spinning.
    const auto base =
        spinning_
            ? session->autoSpinDamage
            : static_cast<float>(player::playerAttributes::valueOf(*session, "minecraft:attack_damage", definitionOf));
    // parity: what the enchantments add rides on the swing but not on the critical, and is put back
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
        session->onGround, session->sprinting, bonus);
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
    // parity: `causeExtraKnockback`: the attribute and the knockback enchantment halved, and the half
    // a sprinting hit adds, along the attacker's facing; the 0.4 every hit is worth was inside it.
    player::pvp::knockMob(*session, *target,
                          player::pvp::knockbackOf(melee, *session, heldStack) +
                              (knockbackAttack ? static_cast<float>(kSprintKnockback) : 0.0F));
    // parity: `isSweepAttack`: a sword, on the ground, walking, and a swing that is neither a
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
    // parity: `HurtByTargetGoal` and `PanicGoal` both read the last thing that hurt it: one sends a
    // monster after whoever swung, the other sends an animal running.
    target->hurtBy = session->entityId();
    entity::entityCombat::rememberPlayer(*target, session->uuid());
    // parity: `Warden.hurtServer`, which adds the angry threshold plus twenty to whoever hit it.
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
    // parity: `itemAttackInteraction`: the mace's smash, the post-attack enchantments, the wear.
    player::pvp::itemAttackInteraction(melee, *world, *session, nullptr, target, heldStack,
                                       PlayerInventory::kHotbarStart + session->inventory.heldSlot());
    session->food.addExhaustion(kAttackExhaustion);
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

void CombatHandler::handleInteract(const wire::protocol::play::InteractPacket& interact) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || session->gamemode == Gamemode::Spectator || world == nullptr) {
        return;
    }
    ServerEntity* entity = world->state().entity(interact.entityId);
    if (entity == nullptr || entity->removed) {
        return;
    }
    // parity: `ItemFrame.interact`, which comes before anything a mob would do with the click.
    if (world::itemFrames::isFrame(entity->type()) && useItemFrame(*session, *entity)) {
        return;
    }
    // parity: `Entity.interact`'s leashes, before the mob's own `mobInteract`, and the knot's own.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr) {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        const bool shears = inHand.has_value() && inHand->id == "minecraft:shears";
        const MobActionSystem::LeashUse use =
            entity->type().path() == "leash_knot"
                ? mobs->useOnKnot(*world, *session, *entity, shears)
                : mobs->useOnLeashable(*world, *session, *entity, inHand.has_value() && inHand->id == "minecraft:lead",
                                       shears);
        switch (use) {
        case MobActionSystem::LeashUse::Pass:
            // parity: `Entity.interact`, the equipment sheared once no lead was there to cut.
            if (shears && shearEquipment(*session, *entity, slot)) {
                return;
            }
            break;
        case MobActionSystem::LeashUse::Done:
            return;
        case MobActionSystem::LeashUse::DamageShears:
            owner_->blockInteraction().wearHeldItem(1);
            return;
        case MobActionSystem::LeashUse::ConsumeOne:
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    // parity: `Villager.mobInteract`, which a villager spawn egg skips.
    if (entity->type() == entity::villagers::type()) {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::optional<storage::StoredItemStack>& held = session->inventory.heldItem();
        if (!mainHand || !held.has_value() || held->id != "minecraft:villager_spawn_egg") {
            owner_->trades().openTrades(*entity, mainHand);
            return;
        }
    }
    // parity: `Minecart.interact` seats whoever clicked it; `MinecartChest` opens its chest instead,
    // and the other carts have no seat.
    if (world::detectorRails::isMinecart(entity->type())) {
        if (entity->type().path() == "minecart") {
            if (!session->sneaking && entity->passengerIds.empty()) {
                mount(*session, *entity);
            }
        } else if (entity->type().path() == "chest_minecart") {
            owner_->menus().openEntityChest(*session, *entity);
        } else if (entity->type().path() == "furnace_minecart" && owner_->options().vehicles != nullptr) {
            // parity: `MinecartFurnace.interact`: a piece of fuel from that hand, spent unless the
            // player has infinite materials.
            const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
            const std::int32_t slot = mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot()
                                               : PlayerInventory::kOffhandSlot;
            const std::optional<storage::StoredItemStack> held = session->inventory.at(slot);
            const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
            const world::realItemTags::TagMembers* itemTags = owner_->options().itemTags;
            const bool fuel = item.has_value() && itemTags != nullptr &&
                              world::realItemTags::contains(*itemTags, "minecraft:furnace_minecart_fuel", *item);
            if (owner_->options().vehicles->fuel(*world, *session, *entity, fuel) &&
                session->gamemode != Gamemode::Creative) {
                storage::StoredItemStack left = *held;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
        }
        return;
    }
    // parity: `AbstractBoat.interact`, a seat for a player not crouching.
    if (entity::boats::isBoat(entity->type())) {
        // parity: `AbstractChestBoat.interact`: a seat first, and the chest to a crouching player or
        // when there is no seat to be had.
        if (!boardBoat(*session, *entity) && entity::boats::hasChest(entity->type()) &&
            (session->sneaking ||
             static_cast<std::int32_t>(entity->passengerIds.size()) >= entity::boats::maxPassengers(entity->type()))) {
            owner_->menus().openEntityChest(*session, *entity);
        }
        return;
    }
    // parity: `Cushion.interact`: a player not sneaking sits on one nobody is on.
    if (world::cushions::isCushion(entity->type())) {
        if (!session->sneaking && entity->passengerIds.empty() && !session->vehicleId.has_value()) {
            mount(*session, *entity);
            owner_->broadcastSound("minecraft:entity.cushion.sit", entity->x, entity->y, entity->z,
                                   SoundSource::Neutral);
        }
        return;
    }
    if (entity::horses::isHorse(entity->type()) &&
        interactMount(*session, *entity, interact.hand == wire::protocol::play::InteractPacket::kMainHand)) {
        return;
    }
    if (tameOrSit(*world, *session, *entity)) {
        return;
    }
    // parity: `Allay.mobInteract`: an item to carry, the item back, or a shard while it dances.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr && entity::allays::isAllay(entity->type())) {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        std::optional<server::world::loot::ItemStack> held;
        if (inHand.has_value() && inHand->count > 0) {
            if (const std::optional<Identifier> id = parsedOrNothing(inHand->id); id.has_value()) {
                held = server::world::loot::ItemStack{
                    .item = *id, .count = inHand->count, .components = inHand->components};
            }
        }
        switch (mobs->useOnAllay(*world, *session, *entity, held, mainHand)) {
        case MobActionSystem::AllayUse::Pass:
            break;
        case MobActionSystem::AllayUse::Done:
            owner_->sendInventory();
            return;
        case MobActionSystem::AllayUse::ConsumeOne:
            // parity: `ItemStack.consume`, which a creative hand keeps.
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    // parity: `Goat.mobInteract`'s bucket of milk.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr && entity->type().path() == "goat") {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        if (mobs->milkGoat(*world, *session, *entity, slot)) {
            owner_->sendInventory();
            return;
        }
    }
    // parity: `PiglinAi.mobInteract`: a gold ingot to admire and barter.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr && entity->type().path() == "piglin") {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        std::optional<server::world::loot::ItemStack> held;
        if (inHand.has_value() && inHand->count > 0) {
            if (const std::optional<Identifier> id = parsedOrNothing(inHand->id); id.has_value()) {
                held = server::world::loot::ItemStack{
                    .item = *id, .count = inHand->count, .components = inHand->components};
            }
        }
        if (mobs->useOnPiglin(*world, *session, *entity, held)) {
            // parity: `consumeAndReturn`, which a creative hand keeps.
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    // parity: `CopperGolem.mobInteract`: the carried item back, shears, wax and a scrape.
    if (MobActionSystem* mobs = owner_->options().mobs;
        mobs != nullptr && entity::copperGolems::isCopperGolem(entity->type())) {
        const std::int32_t slot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        switch (mobs->useOnCopperGolem(*world, *session, *entity,
                                       inHand.has_value() ? parsedOrNothing(inHand->id) : std::nullopt)) {
        case MobActionSystem::GolemUse::Pass:
            break;
        case MobActionSystem::GolemUse::Done:
            return;
        case MobActionSystem::GolemUse::DamageTool:
            owner_->blockInteraction().wearHeldItem(1);
            return;
        case MobActionSystem::GolemUse::ConsumeOne:
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack>& held = session->inventory.at(heldSlot);
    if (!held.has_value()) {
        return;
    }
    const std::optional<Identifier> item = parsedOrNothing(held->id);
    // parity: `ZombieVillager.mobInteract`, where the apple is taken only when the cure starts.
    if (item.has_value() && item->path() == "golden_apple" && entity->type().path() == "zombie_villager") {
        MobActionSystem* mobs = owner_->options().mobs;
        if (mobs != nullptr && mobs->startCure(*world, *entity, session->uuid()) &&
            session->gamemode != Gamemode::Creative) {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session->inventory.set(heldSlot, left.count <= 0 ? std::nullopt : std::optional{left});
            owner_->sendInventorySlot(heldSlot);
        }
        return;
    }
    // parity: `Dolphin.mobInteract`: any fish, a calf grows and a grown one is ready to lead.
    if (item.has_value() && entity->type().path() == "dolphin" && owner_->options().itemTags != nullptr &&
        world::realItemTags::contains(*owner_->options().itemTags, "minecraft:fishes", *item)) {
        owner_->broadcastSound("minecraft:entity.dolphin.eat", entity->x, entity->y, entity->z, SoundSource::Neutral);
        if (entity::breeding::canAgeUp(*entity)) {
            entity::breeding::feedBaby(*entity);
        } else if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr) {
            entity->gotFish = true;
            mobs->poseChanged(*entity);
        }
        if (session->gamemode != Gamemode::Creative) {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session->inventory.set(heldSlot, left.count <= 0 ? std::nullopt : std::optional{left});
            owner_->sendInventorySlot(heldSlot);
        }
        return;
    }
    // parity: `Armadillo.mobInteract`: a brush takes a scute off, and a curled one takes nothing else.
    if (entity->type().path() == "armadillo") {
        MobActionSystem* mobs = owner_->options().mobs;
        if (item.has_value() && item->path() == "brush" && mobs != nullptr && mobs->brushArmadillo(*world, *entity)) {
            owner_->blockInteraction().wearHeldItem(16);
            return;
        }
        if (entity::armadillos::isScared(*entity)) {
            return;
        }
    }
    // parity: `Tadpole.feed`: the frog food takes a tenth of the wait off, in whole seconds.
    if (item.has_value() && entity->type().path() == "tadpole" && owner_->options().itemTags != nullptr &&
        world::realItemTags::contains(*owner_->options().itemTags, "minecraft:frog_food", *item)) {
        const std::int32_t left = std::max(0, entity::mobConversions::kTadpoleTicks - entity->conversionClock);
        entity->conversionClock += static_cast<std::int32_t>(static_cast<float>(left / 20) * 0.1F) * 20;
        if (session->gamemode != Gamemode::Creative) {
            storage::StoredItemStack rest = *held;
            rest.count = held->count - 1;
            session->inventory.set(heldSlot, rest.count <= 0 ? std::nullopt : std::optional{rest});
            owner_->sendInventorySlot(heldSlot);
        }
        return;
    }
    if (!item.has_value() || !entity::breeding::isFood(entity->type(), *item)) {
        return;
    }
    // parity: `Animal.mobInteract` feeds a grown one into love and a calf into growing faster, and
    // does nothing at all to one that is neither.
    const std::optional<world::TriggerItem> used = world::triggerFacts::itemOf(held);
    if (entity::breeding::canFallInLove(*entity)) {
        entity::breeding::feed(*entity, session->uuid().toString());
    } else if (entity::breeding::canAgeUp(*entity)) {
        entity::breeding::feedBaby(*entity);
    } else {
        return;
    }
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire("player_interacted_with_entity", session->name(), session->uuid().toString(),
                       world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(*world, *entity)}},
                                           .items = {{"item", used.value_or(world::TriggerItem{.item = *item})}}});
    }
    if (session->gamemode != Gamemode::Creative) {
        if (held->count <= 1) {
            session->inventory.set(heldSlot, std::nullopt);
        } else {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session->inventory.set(heldSlot, left);
        }
        owner_->sendInventorySlot(heldSlot);
    }
}

bool CombatHandler::useItemFrame(PlayerSession& session, ServerEntity& frame) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const std::optional<server::world::loot::ItemStack> hand =
        item.has_value() ? std::optional{server::world::loot::ItemStack{
                               .item = *item, .count = held->count, .components = held->components}}
                         : std::nullopt;
    // parity: `ItemFrame.interact` refuses a map that already tracks more than 256 markers.
    if (hand.has_value() && !world::itemFrames::shownIn(frame).has_value() && owner_->options().maps != nullptr &&
        !(frame.sourceNbt.has_value() && frame.sourceNbt->getBoolean("Fixed")) &&
        owner_->options().maps->tooManyMarkers(hand->components)) {
        return true;
    }
    const world::itemFrames::Touched touched = world::itemFrames::interact(frame, hand);
    if (touched == world::itemFrames::Touched::Nothing) {
        return false;
    }
    broadcastFrame(frame);
    // parity: both halves of `interact` end in `updateNeighbourForOutputSignal`, which is what tells
    // the comparator reading the frame that its number moved.
    const std::array<std::int32_t, 3> square = world::itemFrames::squareOf(frame);
    world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), square[0], square[1], square[2],
                                                         world::comparatorReading::of(world->state()));
    if (touched == world::itemFrames::Touched::Took && session.gamemode != Gamemode::Creative && held.has_value()) {
        if (held->count <= 1) {
            session.inventory.set(heldSlot, std::nullopt);
        } else {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session.inventory.set(heldSlot, left);
        }
        owner_->sendInventorySlot(heldSlot);
    }
    return true;
}

void CombatHandler::broadcastFrame(const ServerEntity& frame) {
    const std::optional<server::world::loot::ItemStack> shown = world::itemFrames::shownIn(frame);
    std::optional<wire::protocol::play::WireItemStack> drawn;
    if (shown.has_value()) {
        const server::registry::Item* entry = server::registry::items::registry().byName(shown->item);
        if (entry != nullptr) {
            drawn = wire::protocol::play::WireItemStack{.itemId = entry->protocolId, .count = shown->count};
        }
    }
    // parity: `DATA_ITEM` is the frame's own first field and `DATA_ROTATION` the one after it.
    const SetEntityDataPacket packet{
        .entityId = frame.entityId(),
        .entries = {SetEntityDataPacket::Entry{.index = SetEntityDataPacket::kItemFrameStackIndex,
                                               .value = SetEntityDataPacket::ItemValue{.stack = drawn}},
                    SetEntityDataPacket::Entry{
                        .index = SetEntityDataPacket::kItemFrameStackIndex + 1,
                        .value = SetEntityDataPacket::IntValue{.value = world::itemFrames::rotationOf(frame)}}}};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

bool CombatHandler::tameOrSit(const world::LoadedWorld& world, PlayerSession& session, ServerEntity& entity) {
    if (!entity::taming::isTamable(entity.type())) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (entity.owner.has_value()) {
        if (*entity.owner != session.uuid().toString()) {
            return true;
        }
        entity.sitting = !entity.sitting;
        entity::ai::navigation::stop(entity);
        broadcastTameFlags(entity);
        return true;
    }
    if (!item.has_value() || !entity::taming::tamedBy(entity.type(), *item)) {
        return false;
    }
    if (session.gamemode != Gamemode::Creative) {
        if (held->count <= 1) {
            session.inventory.set(heldSlot, std::nullopt);
        } else {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session.inventory.set(heldSlot, left);
        }
        owner_->sendInventorySlot(heldSlot);
    }
    const std::int32_t roll =
        options_.nextDouble ? static_cast<std::int32_t>(options_.nextDouble() * entity::taming::kTameOdds) : 0;
    if (roll != 0) {
        broadcastEntityEvent(entity.entityId(), entity::taming::kEventTamingFailed);
        return true;
    }
    entity.owner = session.uuid().toString();
    // parity: `TamableAnimal.tame` ends in `TAME_ANIMAL.trigger`.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire("tame_animal", session.name(), session.uuid().toString(),
                       world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(world, entity)}}});
    }
    entity.sitting = true;
    entity.targetId.reset();
    entity::ai::navigation::stop(entity);
    broadcastEntityEvent(entity.entityId(), entity::taming::kEventTamingSucceeded);
    broadcastTameFlags(entity);
    return true;
}

void CombatHandler::mount(PlayerSession& session, ServerEntity& vehicle) {
    if (session.vehicleId == vehicle.entityId()) {
        return;
    }
    // parity: `Entity.startRiding` leaves whatever it was on first.
    if (session.vehicleId.has_value()) {
        dismount(session);
    }
    session.vehicleId = vehicle.entityId();
    vehicle.passengerIds.push_back(session.entityId());
    broadcastPassengers(vehicle);
    // parity: `GameEvent.ENTITY_MOUNT`, from the player at where the vehicle is.
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        world::vibrations::emit(world->state(), "entity_mount", vehicle.x, vehicle.y, vehicle.z, world->gameTime(),
                                {.entity = session.uuid()});
    }
    // parity: `Entity.startRiding` ends in `START_RIDING_TRIGGER` for every player now aboard.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire("started_riding", session.name(), session.uuid().toString());
    }
}

void CombatHandler::breakVehicle(PlayerSession& session, ServerEntity& vehicle) {
    world::LoadedWorld* world = owner_->currentWorld();
    VehicleSystem* vehicles = owner_->options().vehicles;
    if (world == nullptr || vehicles == nullptr) {
        return;
    }
    // parity: `Player.attack` against something not alive: the same swing, no critical, and the
    // hit handed to `VehicleEntity.hurtServer`.
    const std::optional<storage::StoredItemStack>& heldStack = session.inventory.heldItem();
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const player::playerAttributes::DefinitionOf definitionOf = [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
    const float strength = player::combat::attackStrengthScale(
        session.ticksSinceSwing, player::playerAttributes::valueOf(session, "minecraft:attack_speed", definitionOf));
    session.ticksSinceSwing = 0;
    const auto base =
        static_cast<float>(player::playerAttributes::valueOf(session, "minecraft:attack_damage", definitionOf));
    const float enchanted = world::weaponDamage::of(heldStack, base, definitionOf);
    const player::combat::Swing swing =
        player::combat::swingOf(strength, base, enchanted, false, session.onGround, session.sprinting);
    if (swing.damage <= 0.0F) {
        return;
    }
    if (swing.knockbackAttack) {
        owner_->broadcastSound(kSoundAttackKnockback, session.x, session.y, session.z, SoundSource::Players);
    }
    const bool landed = vehicles->hurt(*world, vehicle,
                                       VehicleSystem::Hit{.amount = swing.damage,
                                                          .type = world::damageTypes::kPlayerAttack,
                                                          .causeId = session.entityId(),
                                                          .directId = session.entityId(),
                                                          .creativePlayer = session.gamemode == Gamemode::Creative});
    if (!landed) {
        owner_->broadcastSound(kSoundAttackNoDamage, session.x, session.y, session.z, SoundSource::Players);
        return;
    }
    // parity: `causeExtraKnockback`, a push for something that is not alive.
    if (swing.knockbackAttack && !vehicle.removed) {
        const float yaw = session.yaw * (std::numbers::pi_v<float> / 180.0F);
        vehicle.velocityX += static_cast<double>(-wire::common::mth::sin(yaw) * 0.5F);
        vehicle.velocityY += 0.1;
        vehicle.velocityZ += static_cast<double>(wire::common::mth::cos(yaw) * 0.5F);
    }
    owner_->broadcastSound(swing.fullStrength ? kSoundAttackStrong : kSoundAttackWeak, session.x, session.y, session.z,
                           SoundSource::Players);
    if (enchanted - base > 0.0F) {
        broadcastAnimation(vehicle.entityId(), wire::protocol::play::AnimatePacket::kMagicCriticalHit);
    }
    session.food.addExhaustion(kAttackExhaustion);
}

void CombatHandler::breakCushion(PlayerSession& session, ServerEntity& cushion) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    // parity: `BlockAttachedEntity.hurtServer` kills it and `Cushion.dropItem` follows: the sound
    // always, the item only outside creative and with `entity_drops` on.
    for (const std::int32_t passenger : cushion.passengerIds) {
        for (PlayerSession* rider : owner_->playersInSameWorld()) {
            if (rider->entityId() == passenger) {
                dismount(*rider);
                break;
            }
        }
    }
    cushion.removed = true;
    world::vibrations::emit(world->state(), "entity_die", cushion.x, cushion.y, cushion.z, world->gameTime(),
                            {.entity = session.uuid()});
    owner_->broadcastSound("minecraft:entity.cushion.break", cushion.x, cushion.y, cushion.z, SoundSource::Neutral);
    if (session.gamemode == Gamemode::Creative || !world->gameRules().get(world::gameRules::kEntityDrops)) {
        return;
    }
    wire::nbt::NbtCompound components;
    if (cushion.sourceNbt.has_value()) {
        if (const wire::nbt::NbtTag* name = cushion.sourceNbt->find("CustomName"); name != nullptr) {
            components.set("minecraft:custom_name", *name);
        }
    }
    owner_->blockInteraction().spawnDroppedItem(world::cushions::itemOf(cushion), 1, cushion.x, cushion.y, cushion.z,
                                                0.0, 0.0, 0.0, 10, std::move(components));
}

void CombatHandler::breakFrame(PlayerSession& session, ServerEntity& frame) {
    world::LoadedWorld* world = owner_->currentWorld();
    // parity: a fixed frame takes nothing from a plain hit.
    if (world == nullptr || (frame.sourceNbt.has_value() && frame.sourceNbt->getBoolean("Fixed"))) {
        return;
    }
    const std::optional<server::world::loot::ItemStack> shown = world::itemFrames::shownIn(frame);
    // parity: `dropItem` puts nothing in the world for a creative player or with `entity_drops` off,
    // and the frame is emptied either way.
    const bool drops = session.gamemode != Gamemode::Creative && world->gameRules().get(world::gameRules::kEntityDrops);
    if (shown.has_value()) {
        static_cast<void>(frame.sourceNbt->remove("Item"));
        broadcastFrame(frame);
        // parity: `dropItem` takes the map's marker off unless drops are off, where only a frame
        // coming down on its own does.
        if (world->gameRules().get(world::gameRules::kEntityDrops) && owner_->options().maps != nullptr) {
            owner_->options().maps->frameLostMap(shown->components, frame);
        }
        if (drops) {
            owner_->blockInteraction().spawnDroppedItem(shown->item, shown->count, frame.x, frame.y, frame.z, 0.0, 0.0,
                                                        0.0, kDefaultPickupDelay, shown->components);
        }
    } else {
        frame.removed = true;
        if (drops) {
            owner_->blockInteraction().spawnDroppedItem(frame.type(), 1, frame.x, frame.y, frame.z);
        }
    }
    // parity: both halves raise `GameEvent.BLOCK_CHANGE`, and a comparator reading the frame has to
    // hear that it is empty now.
    const std::array<std::int32_t, 3> square = world::itemFrames::squareOf(frame);
    world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), square[0], square[1], square[2],
                                                         world::comparatorReading::of(world->state()));
    return;
}

void CombatHandler::killEntity(ServerEntity& entity) {
    owner_->broadcastSound(entity::entityCombat::deathSound(entity), entity.x, entity.y, entity.z,
                           SoundSource::Hostile);
    broadcastEntityEvent(entity.entityId(), wire::protocol::play::EntityEventPacket::kLivingDeath);
    entity.killed = true;
    entity.dying = true;
}

void CombatHandler::broadcastEntityEvent(std::int32_t entityId, std::int32_t event) {
    const wire::protocol::play::EntityEventPacket packet{.entityId = entityId,
                                                         .eventId = static_cast<std::int8_t>(event)};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastDamageEvent(const ServerEntity& entity, const Identifier& damageType,
                                         std::int32_t causeEntityId) {
    const std::vector<registry::vanillaRegistryData::Entry>& types = registry::vanillaRegistryData::damageTypes();
    std::int32_t registryId = -1;
    for (std::size_t index = 0; index < types.size(); index++) {
        if (types[index].first == damageType.path()) {
            registryId = static_cast<std::int32_t>(index);
            break;
        }
    }
    if (registryId < 0) {
        return;
    }
    const wire::protocol::play::DamageEventPacket packet{.entityId = entity.entityId(),
                                                         .damageType = registryId,
                                                         .causeEntityId = causeEntityId,
                                                         .directEntityId = causeEntityId};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::DamageEventPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastMotion(const ServerEntity& entity) {
    const wire::protocol::play::SetEntityMotionPacket packet{.entityId = entity.entityId(),
                                                             .velocityX = entity.velocityX,
                                                             .velocityY = entity.velocityY,
                                                             .velocityZ = entity.velocityZ};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetEntityMotionPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastTameFlags(const ServerEntity& entity) {
    const SetEntityDataPacket packet{.entityId = entity.entityId(),
                                     .entries = {SetEntityDataPacket::Entry{
                                         .index = static_cast<std::uint8_t>(entity::taming::kFlagsIndex),
                                         .value = SetEntityDataPacket::ByteValue{
                                             .value = static_cast<std::int8_t>(entity::taming::flagsOf(entity))}}}};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastPassengers(const ServerEntity& vehicle) {
    const wire::protocol::play::SetPassengersPacket packet{.vehicleId = vehicle.entityId(),
                                                           .passengerIds = vehicle.passengerIds};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPassengers) {
            player->connection().sendPassengers(vehicle.entityId(), vehicle.passengerIds);
            continue;
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastAnimation(std::int32_t entityId, std::int32_t animation) {
    const wire::protocol::play::AnimatePacket packet{.entityId = entityId,
                                                     .action = static_cast<std::uint8_t>(animation)};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::AnimatePacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastParticle(std::string_view particle, double x, double y, double z, std::int32_t count) {
    const server::registry::ParticleType* type =
        server::registry::particleTypes::of(Identifier::vanilla(std::string{particle}));
    if (type == nullptr) {
        return;
    }
    const wire::protocol::play::LevelParticlesPacket packet{
        .particleTypeId = type->protocolId, .x = x, .y = y, .z = z, .count = count};
    const std::string name = type->id.toString();
    for (const PlayerSession* player : owner_->playersInSameWorld()) {
        player::showParticleFor(*player, name, packet);
    }
}

} // namespace basalt::server::runtime
