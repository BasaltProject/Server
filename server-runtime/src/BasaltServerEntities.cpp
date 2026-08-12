#include "BasaltServerTickShared.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BlockTests.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/LightningBoltSystem.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/AreaEffectClouds.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/DeathDrops.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/PrimedTnt.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/AttachedEntities.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerDrops.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <random>
#include <set>
#include <string>

namespace basalt::server::runtime {
using namespace basaltServerTick;

void BasaltServer::tickEntities(world::LoadedWorld& world, const sched::Region& region) {
    std::vector<player::PlayerSession*> watchers;
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    // parity: `LivingEntity.pushEntities` on each player's own turn.
    for (player::PlayerSession* player : watchers) {
        if (region.ownsPosition(player->x, player->z)) {
            // parity: `baseTick`'s currents come before `aiStep`'s pushes.
            pushes_->pushByFluid(world, *player);
            pushes_->pushByPlayer(world, *player);
        }
    }

    // parity: `BlockEntity.preRemoveSideEffects`.
    // parity: `StructureBlock.trigger`.
    for (const std::array<std::int32_t, 3>& at : world.state().takeStructureTriggers()) {
        if (structureBlocks_ != nullptr) {
            structureBlocks_->trigger(world, at);
        }
    }
    for (world::WorldState::Spilled& spilled : world.state().takeSpilledContents()) {
        if (spilled.lootTable.has_value()) {
            world::containerLoot::fill(world, vanillaData_.lootTables, *spilled.lootTable, spilled.lootSeed,
                                       spilled.lootSlots, false, {spilled.x, spilled.y, spilled.z});
            for (std::optional<server::world::loot::ItemStack>& slot : spilled.lootSlots) {
                if (slot.has_value() && slot->count > 0) {
                    spilled.stacks.push_back(std::move(*slot));
                }
            }
        }
        random::RandomSource random = random::RandomSource::legacy(scratchSeed());
        for (const server::world::loot::ItemStack& stack : spilled.stacks) {
            // parity: `dropItemStack` from the square, not its centre, in handfuls.
            world::containerDrops::dropItemStack(world.state(), spilled.x, spilled.y, spilled.z, stack, random,
                                                 [this] { return playerRegistry_.allocateEntityId(); });
        }
        // parity: `AbstractFurnaceBlockEntity.preRemoveSideEffects` pays the cooking out at the centre.
        for (const auto& [name, count] : spilled.recipesUsed) {
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                wire::common::Identifier::parse(name);
            const server::world::crafting::Recipe* recipe =
                id.has_value() ? vanillaData_.recipes.recipeFor(*id) : nullptr;
            if (recipe != nullptr) {
                const ExperienceOrbSystem::ReasonScope reason{ExperienceOrbSystem::Reason::Furnace};
                orbs_->award(world, spilled.x + 0.5, spilled.y + 0.5, spilled.z + 0.5,
                             world::furnaces::experienceFor(count, recipe->experience, random));
            }
        }
    }

    const entity::entityNbt::AllocateEntityId allocateEntityId = [this] { return playerRegistry_.allocateEntityId(); };
    const entity::entityNbt::AllocateEntityIdFor allocateEntityIdFor = [this](const wire::common::Identifier& type) {
        return playerRegistry_.allocateEntityIds(entity::enderDragon::idSpan(type));
    };
    const entity::entityNbt::ProtocolTypeOf protocolTypeOf =
        [](const wire::common::Identifier& type) -> std::optional<std::int32_t> {
        const registry::EntityTypeEntry* entry = registry::entityTypes::of(type);
        return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
    };
    // parity: `ItemEntity.onDestroyed`, a shulker box that is lost spills what it held.
    random::RandomSource spillRandom = random::RandomSource::legacy(scratchSeed());
    for (const world::DroppedItem& gone : world.state().takeDestroyedItems()) {
        const wire::nbt::NbtCompound* held = gone.components.getCompound(std::string{world::shulkers::kContainer});
        if (held == nullptr || !world.state().gameplay().items.shulkerBoxDropContentsWhenDestroyed) {
            continue;
        }
        for (const std::optional<server::world::loot::ItemStack>& slot :
             world::blockInventory::read(*held, world::shulkers::kSize)) {
            if (!slot.has_value()) {
                continue;
            }
            world::DroppedItem spilled{.entityId = playerRegistry_.allocateEntityId(),
                                       .uuid = wire::common::Uuid::random(),
                                       .item = slot->item,
                                       .count = slot->count,
                                       .x = gone.x,
                                       .y = gone.y,
                                       .z = gone.z,
                                       .velocityX = (spillRandom.nextDouble() - 0.5) * 0.2,
                                       .velocityY = 0.2,
                                       .velocityZ = (spillRandom.nextDouble() - 0.5) * 0.2};
            spilled.components = slot->components;
            static_cast<void>(world.state().spawnItem(std::move(spilled)));
        }
    }
    // Entity ids are handed out by the tick loop, while chunks load on a generation thread.
    for (const wire::nbt::NbtCompound& tag : world.state().takePendingEntities()) {
        // An item on the floor is its own thing here, not a `ServerEntity`, so it comes back its own way.
        if (const std::optional<world::DroppedItem> item = world::droppedItemNbt::decode(tag, allocateEntityId);
            item.has_value()) {
            static_cast<void>(world.state().spawnItem(*item));
            continue;
        }
        for (std::unique_ptr<entity::ServerEntity>& restored :
             entity::entityNbt::decodeTree(tag, allocateEntityIdFor, protocolTypeOf)) {
            static_cast<void>(world.state().spawnEntity(std::move(restored)));
        }
    }
    // Structure mobs are generated off-thread and cannot allocate runtime ids, so they become live entities here.
    for (const server::world::ChunkGeneratedEntity& descriptor : world.state().takePendingGeneratedEntities()) {
        for (std::unique_ptr<entity::ServerEntity>& generated :
             entity::entityNbt::decodeGeneratedTree(descriptor, allocateEntityIdFor, protocolTypeOf)) {
            static_cast<void>(world.state().spawnEntity(std::move(generated)));
        }
    }

    // parity: `LivingEntity.tickDeath`.
    for (entity::ServerEntity* dying : world.state().entities()) {
        // parity: `EnderDragon.tickDeath` is its own two hundred ticks, run from its own turn.
        if (dying->removed || !dying->dying || dying->dragon != nullptr) {
            continue;
        }
        ++dying->deathTime;
        // parity: `Creaking.tickDeath`.
        if (dying->creaking != nullptr && dying->creaking->tearingDown && dying->creaking->home.has_value()) {
            if (dying->deathTime > entity::creakings::kTearDownTicks && mobActions_ != nullptr) {
                mobActions_->tearDownCreaking(world, *dying);
            }
            continue;
        }
        if (dying->deathTime < entity::kDeathTicks) {
            continue;
        }
        // parity: `AbstractCubeMob.remove`.
        random::RandomSource splitRandom = random::RandomSource::legacy(scratchSeed());
        const std::vector<entity::cubeMobs::Split> splits = entity::cubeMobs::splitsOf(*dying, splitRandom);
        std::size_t splitCount = splits.size();
        if (!splits.empty()) {
            const server::plugins::PluginEvent* asked = world.events().ask(
                &server::plugins::pluginEventTypes::entitySplit, [&](server::plugins::PluginEvent& event) {
                    plugins::hookEvents::entity(event, world.name(), *dying);
                    event.set("count", static_cast<std::int64_t>(splits.size()));
                });
            if (asked != nullptr && asked->cancelled()) {
                splitCount = 0;
            } else if (const auto* edited =
                           asked == nullptr ? nullptr : std::get_if<std::int64_t>(&asked->get("count"));
                       edited != nullptr) {
                splitCount = std::min(splitCount, static_cast<std::size_t>(std::max<std::int64_t>(0, *edited)));
            }
        }
        std::size_t splitIndex = 0;
        for (const entity::cubeMobs::Split& split : splits) {
            if (spawning_ == nullptr || splitIndex++ >= splitCount) {
                break;
            }
            static_cast<void>(spawning_->spawnMob(
                world, dying->type(), split.x, split.y, split.z, split.yaw, "triggered",
                [&split](entity::ServerEntity& half) { entity::cubeMobs::applySize(half, split.size); }));
        }
        dying->removed = true;
    }

    // One snapshot per tick: `entities()` copies under the world lock and per-entity calls made a lock convoy.
    std::vector<entity::ServerEntity*> living;
    for (entity::ServerEntity* candidate : world.state().entities()) {
        if (!candidate->removed) {
            living.push_back(candidate);
        }
    }

    // Projectiles are created in many places; the newest ids are the ones nobody has been asked about yet.
    {
        static constexpr std::array<std::string_view, 19> kProjectiles{
            "arrow",          "spectral_arrow",    "trident",         "snowball",         "egg",
            "ender_pearl",    "experience_bottle", "splash_potion",   "lingering_potion", "fireball",
            "small_fireball", "dragon_fireball",   "wither_skull",    "wind_charge",      "breeze_wind_charge",
            "llama_spit",     "shulker_bullet",    "firework_rocket", "fishing_bobber"};
        const auto known = launchWatermarks_.find(world.name());
        std::int32_t newest = known == launchWatermarks_.end() ? 0 : known->second;
        const std::int32_t previous = newest;
        for (entity::ServerEntity* candidate : living) {
            newest = std::max(newest, candidate->entityId());
            if (known == launchWatermarks_.end() || candidate->entityId() <= previous ||
                std::ranges::find(kProjectiles, candidate->type().path()) == kProjectiles.end()) {
                continue;
            }
            const bool allowed = eventSink_.allowed(
                &server::plugins::pluginEventTypes::entityLaunch, [&](server::plugins::PluginEvent& filled) {
                    filled.set("entity_id", static_cast<std::int64_t>(candidate->entityId()));
                    filled.set("entity_type", candidate->type().toString());
                    filled.set("world", world.name());
                    filled.set("x", candidate->x);
                    filled.set("y", candidate->y);
                    filled.set("z", candidate->z);
                    filled.set("shooter_id", static_cast<std::int64_t>(candidate->ownerId.value_or(-1)));
                });
            if (!allowed) {
                candidate->removed = true;
            }
        }
        launchWatermarks_.insert_or_assign(world.name(), newest);
    }

    // parity: `TargetingConditions.forCombat`.
    const double playerWidth = registry::entityTypeShapes::widthOf(playerType());
    const double playerHeight = registry::entityTypeShapes::heightOf(playerType());
    std::vector<entity::ai::PlayerView> views;
    for (player::PlayerSession* player : watchers) {
        if (region.ownsPosition(player->x, player->z)) {
            player->movedHorizontally =
                std::hypot(player->x - player->seenAt[0], player->z - player->seenAt[1]) > static_cast<double>(1.0E-5F);
            player->seenAt = {player->x, player->z};
            std::optional<wire::common::Identifier> heldItem;
            const std::optional<storage::StoredItemStack>& held = player->inventory.heldItem();
            if (held.has_value()) {
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                    wire::common::Identifier::parse(held->id);
                if (parsed.has_value()) {
                    heldItem = *parsed;
                }
            }
            const std::optional<storage::StoredItemStack>& head =
                player->inventory.at(player::PlayerInventory::kArmourStart);
            const auto headItem = wire::common::Identifier::parse(head.has_value() ? head->id : std::string{});
            const bool disguised =
                headItem.has_value() &&
                world::realItemTags::contains(vanillaData_.itemTags, "minecraft:gaze_disguise_equipment", *headItem);
            views.push_back(entity::ai::PlayerView{
                .entityId = player->entityId(),
                .uuid = player->uuid().toString(),
                .x = player->x,
                .y = player->y,
                .z = player->z,
                .width = playerWidth,
                .height = playerHeight,
                .targetable = isTargetable(*player),
                .spectator = isSpectator(*player),
                .heldItem = std::move(heldItem),
                .sneaking = player->sneaking,
                .sprinting = player->sprinting,
                .swimming = player->swimming,
                .yaw = player->yaw,
                .pitch = player->pitch,
                .vehicleId = player->vehicleId,
                .movedHorizontally = player->movedHorizontally,
                .facing = wire::common::mth::floor(static_cast<double>(player->yaw / 90.0F) + 0.5) & 3,
                .sleeping = player->sleepingBed.has_value(),
                .sleepTicks = player->sleepTicks,
                .gazeDisguised = disguised,
                .flying = player->flying,
                .inWater = feetInWater(world, *player)});
        }
    }

    if (region.isPrimary()) {
        orbs_->tickPlayers(watchers);
        if (mobActions_ != nullptr) {
            mobActions_->forgiveDeadPlayers(world, watchers);
            mobActions_->tickShoulders(world, watchers);
            mobActions_->emitPlayerMovement(world, watchers);
        }
    }
    // parity: `ServerLevel.tick` runs the dragon fight before any entity takes its turn.
    if (world.dragonFight() != nullptr && region.ownsPosition(0.0, 0.0)) {
        endFight_->tickFight(world, watchers, living);
    }

    random::RandomSource random = random::RandomSource::legacy(scratchSeed());
    const world::cushions::Tagged attachedTags = [this](std::string_view tag, const wire::common::Identifier& block) {
        return world::realBlockTags::contains(vanillaData_.blockTags, tag, block);
    };
    // Built lazily; entities stop one chunk sooner than blocks (`ChunkLevel.byStatus`).
    std::optional<std::unordered_set<std::int64_t>> entityTicking;
    const auto ticking = [this, &entityTicking, &world, &region](double atX, double atZ) {
        if (!entityTicking.has_value()) {
            std::unordered_set<std::int64_t> built;
            for (const std::array<std::int32_t, 2>& at : chunkSimulation_.simulatedChunks(world, &region)) {
                built.insert((static_cast<std::int64_t>(at[0]) << 32) | static_cast<std::uint32_t>(at[1]));
            }
            entityTicking = std::move(built);
        }
        const auto chunkX = static_cast<std::int32_t>(std::floor(atX)) >> 4;
        const auto chunkZ = static_cast<std::int32_t>(std::floor(atZ)) >> 4;
        return entityTicking->contains((static_cast<std::int64_t>(chunkX) << 32) | static_cast<std::uint32_t>(chunkZ));
    };
    for (entity::ServerEntity* entity : living) {
        if (entity->removed || MobTickSystem::hasActiveVehicle(world, *entity)) {
            continue;
        }
        // parity: `Mob.aiStep`.
        if (entity->dying && !entity::enderDragon::isDragon(entity->type())) {
            // parity: a dying wither is still there, and so is its bar, until the body goes.
            if (WitherSystem::isWither(*entity)) {
                withers_->keepBar(world, *entity);
            }
            continue;
        }
        // An entity may walk out of this region mid-turn; repartitioning only happens between ticks, so that is fine.
        if (!region.ownsPosition(entity->x, entity->z)) {
            continue;
        }
        // parity: `ServerLevel`.
        if (!ticking(entity->x, entity->z)) {
            continue;
        }
        if (entity->sourceNbt.has_value() && entity->type().path() == "armor_stand") {
            if (entity->sourceNbt->getBoolean("BasaltNoTick")) {
                continue;
            }
            if (entity->sourceNbt->getBoolean("BasaltNoMove")) {
                entity->velocityX = entity->velocityY = entity->velocityZ = 0.0;
            }
        }
        if (entity->type() == world::lightning::bolt()) {
            lightning_->tick(world, watchers, living, *entity, random);
            continue;
        }
        if (entity->type() == entity::primedTnt::type()) {
            if (!entity::primedTnt::tick(world.state(), *entity)) {
                continue;
            }
            entity->removed = true;
            explosions_->blowUp(world, watchers, entity->x, entity->y + entity::primedTnt::kExplosionOffset, entity->z,
                                entity->explosionPower > 0.0F ? entity->explosionPower
                                                              : entity::primedTnt::kExplosionPower,
                                "tnt", entity->entityId(), {}, {}, entity->incendiary.value_or(false));
            // parity: `GameEvent.EXPLODE`, which is fifteen: the top of the scale.
            world::vibrations::emit(world.state(), "explode", entity->x, entity->y, entity->z, world.gameTime(),
                                    {.entity = entity->uuid()});
            continue;
        }
        if (entity::enderDragon::isDragon(entity->type())) {
            endFight_->tickDragon(world, watchers, living, *entity);
            continue;
        }
        if (ExperienceOrbSystem::isOrb(*entity)) {
            orbs_->tick(world, watchers, living, *entity);
            continue;
        }
        if (entity::areaEffectClouds::isCloud(entity->type())) {
            clouds_->tick(world, watchers, living, *entity);
            continue;
        }
        if (WitherSystem::isWither(*entity) && !entity->dying) {
            withers_->beforeTurn(world, living, *entity);
            mobTicks_->tickTree(world, *entity, watchers, views, living, random);
            withers_->afterTurn(world, watchers, living, *entity);
            continue;
        }
        if (FishingSystem::isHook(*entity)) {
            fishing_->tick(world, watchers, living, *entity);
            continue;
        }
        // parity: `EndCrystal.tick`.
        if (world::endCrystals::isCrystal(entity->type())) {
            // Purpur's crystal cramming: more crystals than the limit inside one another set the crowd off.
            if (const std::int32_t limit = world.state().gameplay().blocks.endCrystalCrammingAmount; limit > 0) {
                std::int32_t crowd = 0;
                for (const entity::ServerEntity* other : world.state().entities()) {
                    crowd += !other->removed && world::endCrystals::isCrystal(other->type()) &&
                             std::abs(other->x - entity->x) < 2.0 && std::abs(other->y - entity->y) < 2.0 &&
                             std::abs(other->z - entity->z) < 2.0;
                }
                if (crowd > limit) {
                    combat_->hitCrystal(world, watchers, *entity, false);
                    continue;
                }
            }
            if (world.dragonFight() != nullptr) {
                const std::int32_t delay =
                    world.settings().rules.gameplay.fireTickDelay + random.nextInt(world::fires::kDelaySpread);
                if (const std::optional<std::int32_t> lit =
                        world::endCrystals::keepFireLit(world.state(), *entity, delay);
                    lit.has_value()) {
                    broadcaster_->announceRedstone(world, watchers,
                                                   world::BlockChange{static_cast<std::int32_t>(std::floor(entity->x)),
                                                                      static_cast<std::int32_t>(std::floor(entity->y)),
                                                                      static_cast<std::int32_t>(std::floor(entity->z)),
                                                                      *lit,
                                                                      {}});
                }
            }
            continue;
        }
        if (world::enderEyes::isEye(*entity)) {
            world::enderEyes::tick(world, watchers, playerRegistry_, *entity, scratchSeed());
            continue;
        }
        if (entity::fireworkRockets::isRocket(*entity)) {
            fireworks_->tick(world, watchers, living, *entity);
            continue;
        }
        if (entity::shulkers::isBullet(entity->type())) {
            mobActions_->tickShulkerBullet(world, watchers, living, *entity, random);
            continue;
        }
        const entity::projectiles::Kind* projectile = entity::projectiles::kindOf(entity->type());
        if (projectile != nullptr) {
            combat_->tickProjectile(world, watchers, living, *entity, *projectile);
            // parity: a pearl thrown through a gateway comes out at its exit and stops dead.
            if (!entity->removed && entity->type().path() == "ender_pearl") {
                const double half = entity->width() / 2.0;
                if (const std::optional<std::array<double, 3>> exit = environment_->enterGateway(
                        world, {entity->x - half, entity->y, entity->z - half, entity->x + half,
                                entity->y + entity->height(), entity->z + half});
                    exit.has_value()) {
                    entity->x = (*exit)[0];
                    entity->y = (*exit)[1];
                    entity->z = (*exit)[2];
                    entity->velocityX = 0.0;
                    entity->velocityY = 0.0;
                    entity->velocityZ = 0.0;
                }
            }
            continue;
        }
        // parity: `AbstractMinecart.tick`: on a rail it follows the track, and off one it falls.
        if (world::detectorRails::isMinecart(entity->type())) {
            const std::array<double, 3> cartFrom{entity->x, entity->y, entity->z};
            vehicles_->tickMinecart(world, *entity);
            announceVehicleMove(world, *entity, cartFrom);
            if (entity->removed) {
                continue;
            }
            player::PlayerSession* rider = nullptr;
            for (player::PlayerSession* player : watchers) {
                if (!entity->passengerIds.empty() && player->entityId() == entity->passengerIds.front()) {
                    rider = player;
                    break;
                }
            }
            // The rider's position comes from the cart, so chunks load around the same answer the client draws.
            if (rider != nullptr) {
                rider->x = entity->x;
                rider->y = entity->y;
                rider->z = entity->z;
            }
            // parity: `MinecartHopper.tick`.
            static_cast<void>(world::hoppers::tickCart(world.state(), *entity, world.gameTime()));
            const std::int32_t cartY = wire::common::mth::floor(entity->y);
            for (std::int32_t railX = wire::common::mth::floor(entity->x - world::detectorRails::kCartHalfWidth);
                 railX <= wire::common::mth::floor(entity->x + world::detectorRails::kCartHalfWidth); ++railX) {
                for (std::int32_t railZ = wire::common::mth::floor(entity->z - world::detectorRails::kCartHalfWidth);
                     railZ <= wire::common::mth::floor(entity->z + world::detectorRails::kCartHalfWidth); ++railZ) {
                    if (const world::detectorRails::Checked pressed = world::detectorRails::check(
                            world.state(), railX, cartY, railZ, living, world::detectorRails::Caller::Entity);
                        pressed.ran) {
                        if (pressed.change.has_value()) {
                            broadcaster_->announceRedstone(world, watchers, *pressed.change);
                        }
                        // parity: `checkPressed` tells the comparators whatever it decided.
                        world::redstoneComponents::scheduleComparatorsAround(
                            world.state(), world.name(), railX, cartY, railZ,
                            world::comparatorReading::of(world.state()));
                    }
                }
            }
            std::set<std::int32_t> visited;
            mobTicks_->tickPassengers(world, *entity, watchers, views, living, random, visited);
            continue;
        }
        // parity: `BlockAttachedEntity.tick`.
        if (world::attachedEntities::isAttached(entity->type()) && world::attachedEntities::dueForCheck(*entity) &&
            !world::attachedEntities::survives(world.state(), *entity, attachedTags)) {
            // parity: `ItemFrame.dropItem(level, null, true)` drops the framed map's marker either way.
            if (maps_ != nullptr && world::itemFrames::isFrame(entity->type())) {
                if (const std::optional<server::world::loot::ItemStack> held = world::itemFrames::shownIn(*entity);
                    held.has_value()) {
                    maps_->frameLostMap(held->components, *entity);
                }
            }
            world::attachedEntities::comeOff(world, *entity, watchers, random,
                                             [this] { return playerRegistry_.allocateEntityId(); });
            continue;
        }
        // parity: `AbstractBoat.tick`.
        if (entity::boats::isBoat(entity->type())) {
            const std::array<double, 3> boatFrom{entity->x, entity->y, entity->z};
            tickBoat(world, watchers, living, *entity);
            announceVehicleMove(world, *entity, boatFrom);
            if (mobActions_ != nullptr) {
                mobActions_->tickLeash(world, *entity);
            }
        }
        // parity: `Mob.aiStep`, which is what makes an animal wander rather than stand there.
        if (entity::mobs::isMob(entity->type())) {
            mobTicks_->tickTree(world, *entity, watchers, views, living, random);
        } else {
            std::set<std::int32_t> visited;
            mobTicks_->tickPassengers(world, *entity, watchers, views, living, random, visited);
        }
    }
    // Deaths land this turn, not the next: an arrow killing a pig is heard by vanilla's sensor on the same tick.
    tickDeaths(world, region);
}

} // namespace basalt::server::runtime
