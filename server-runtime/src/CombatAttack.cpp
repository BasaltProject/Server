#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/DragonState.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerSweep.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
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
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

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
constexpr float kVehicleDamagePerPoint = 10.0F;
constexpr float kVehicleBreakDamage = 40.0F;

constexpr double kSprintKnockback = 0.5;
constexpr double kSweepSpeedLimit = 0.003;
constexpr double kSweepParticleHeight = 1.0;
constexpr std::string_view kSwordSuffix = "_sword";
constexpr std::string_view kParticleSweep = "sweep_attack";

constexpr std::string_view kSoundAttackKnockback = "entity.player.attack.knockback";
constexpr std::string_view kSoundAttackNoDamage = "entity.player.attack.nodamage";
constexpr std::string_view kSoundAttackCrit = "entity.player.attack.crit";
constexpr std::string_view kSoundAttackStrong = "entity.player.attack.strong";
constexpr std::string_view kSoundAttackWeak = "entity.player.attack.weak";
constexpr std::string_view kSoundAttackSweep = "entity.player.attack.sweep";

constexpr float kAttackExhaustion = 0.1F;
constexpr std::int32_t kPanicTicks = 40;
constexpr float kHalfCircle = 180.0F;

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
    } else if (!owner_->withinAttackReach(*session, *target)) {
        return;
    }
    if (hooks_.attackAllowed && !hooks_.attackAllowed(*session, target->entityId())) {
        return;
    }
    if (world::detectorRails::isMinecart(target->type())) {
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

    const std::optional<storage::StoredItemStack>& heldStack = session->inventory.heldItem();
    const std::optional<Identifier> weapon = heldStack.has_value() ? parsedOrNothing(heldStack->id) : std::nullopt;
    const server::registry::WeaponStats* stats =
        weapon.has_value() ? server::registry::itemWeapons::of(*weapon) : nullptr;
    const float strength = player::combat::attackStrengthScale(session->ticksSinceSwing,
                                                               server::registry::itemWeapons::kBaseAttackSpeed +
                                                                   (stats == nullptr ? 0.0 : stats->attackSpeed));
    session->ticksSinceSwing = 0;

    const auto base = static_cast<float>(server::registry::itemWeapons::kBaseAttackDamage +
                                         (stats == nullptr ? 0.0 : stats->attackDamage));
    // parity: what the enchantments add rides on the swing but not on the critical, and is put back
    // on top after the charge and the critical have had the base to themselves.
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const float enchanted = world::weaponDamage::of(heldStack, base, [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    });
    const player::combat::Swing swing =
        player::combat::swingOf(strength, base, enchanted, fallDistance > 0.0, session->onGround, session->sprinting);
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
        entity::entityCombat::hurt(*target, damage, session->x, session->z, {}, "attack", session->name(), true);
    if (!result.landed) {
        owner_->broadcastSound(kSoundAttackNoDamage, session->x, session->y, session->z, SoundSource::Players);
        return;
    }
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        world::triggerFacts::playerHurtEntity(*triggers, *world, *session, *target, damage,
                                              Identifier::vanilla("player_attack"));
    }
    // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` at whoever took the hit.
    world::vibrations::emit(world->state(), "entity_damage", target->x, target->y, target->z, world->gameTime(),
                            {.entity = target->uuid()});
    // parity: `causeExtraKnockback`, which is the half a sprinting hit adds and nothing else without
    // enchantments: the 0.4 every hit is worth was already taken inside the hit.
    if (knockbackAttack) {
        entity::entityCombat::knockback(*target, kSprintKnockback, session->x - target->x, session->z - target->z);
    }
    // parity: `isSweepAttack`: a sword, on the ground, walking, and a swing that is neither a
    // critical nor a sprinting one.
    const bool sweep = fullStrength && !critical && !knockbackAttack && session->onGround &&
                       owner_->lastStepSquared < kSweepSpeedLimit * kSweepSpeedLimit && weapon.has_value() &&
                       weapon->path().ends_with(kSwordSuffix);
    if (sweep) {
        sweepAround(*session, *target, *world, strength);
    }
    broadcastDamageEvent(*target, world::damageTypes::kPlayerAttack, session->entityId());
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
    // parity: `ItemStack.hurtEnemy` counts a `weapon` as used, and `postHurtEnemy` wears it.
    if (const std::optional<storage::StoredItemStack> weapon = session->inventory.heldItem();
        weapon.has_value() && item::itemWear::costPerAttack(*weapon) > 0) {
        player::statAwards::used(*session, weapon->id);
        owner_->blockInteraction().wearHeldItem(item::itemWear::costPerAttack(*weapon));
    }
    session->food.addExhaustion(kAttackExhaustion);
    if (result.died) {
        killEntity(*target);
    } else {
        owner_->broadcastSound(entity::entityCombat::hurtSound(*target), target->x, target->y, target->z,
                               SoundSource::Hostile);
    }
}

void CombatHandler::sweepAround(PlayerSession& session, const ServerEntity& target, world::LoadedWorld& world,
                                float strength) {
    owner_->broadcastSound(kSoundAttackSweep, session.x, session.y, session.z, SoundSource::Players);
    for (const player::SweepHit& hit : player::sweepAround(session, world, target, strength)) {
        ServerEntity* caught = world.state().entity(hit.entityId);
        if (caught == nullptr) {
            continue;
        }
        broadcastDamageEvent(*caught, world::damageTypes::kPlayerAttack, session.entityId());
        broadcastMotion(*caught);
        if (hit.died) {
            killEntity(*caught);
        }
    }
    // parity: the sweep's own particle, a block ahead of where the player is looking.
    const float yaw = session.yaw * std::numbers::pi_v<float> / kHalfCircle;
    broadcastParticle(kParticleSweep, session.x - static_cast<double>(std::sin(yaw)), session.y + kSweepParticleHeight,
                      session.z + static_cast<double>(std::cos(yaw)), 0);
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
    // parity: `Villager.mobInteract`, which a villager spawn egg skips.
    if (entity->type() == entity::villagers::type()) {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::optional<storage::StoredItemStack>& held = session->inventory.heldItem();
        if (!mainHand || !held.has_value() || held->id != "minecraft:villager_spawn_egg") {
            owner_->trades().openTrades(*entity, mainHand);
            return;
        }
    }
    // parity: `AbstractMinecart.interact`, which seats whoever clicked it.
    if (world::detectorRails::isMinecart(entity->type())) {
        mount(*session, *entity);
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
    if (tameOrSit(*world, *session, *entity)) {
        return;
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
    if (session.vehicleId.has_value()) {
        return;
    }
    session.vehicleId = vehicle.entityId();
    vehicle.passengerIds.push_back(session.entityId());
    broadcastPassengers(vehicle);
    // parity: `Entity.startRiding` ends in `START_RIDING_TRIGGER` for every player now aboard.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire("started_riding", session.name(), session.uuid().toString());
    }
}

void CombatHandler::breakVehicle(PlayerSession& session, ServerEntity& vehicle) {
    broadcastDamageEvent(vehicle, world::damageTypes::kPlayerAttack, session.entityId());
    if (session.gamemode != Gamemode::Creative) {
        vehicle.vehicleDamage += kVehicleDamagePerPoint;
        if (vehicle.vehicleDamage <= kVehicleBreakDamage) {
            return;
        }
        owner_->blockInteraction().spawnDroppedItem(vehicle.type(), 1, vehicle.x, vehicle.y, vehicle.z);
    }
    for (const std::int32_t passenger : vehicle.passengerIds) {
        for (PlayerSession* rider : owner_->playersInSameWorld()) {
            if (rider->entityId() == passenger) {
                dismount(*rider);
                break;
            }
        }
    }
    vehicle.removed = true;
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
