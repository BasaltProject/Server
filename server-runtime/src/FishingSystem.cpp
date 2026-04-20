#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/entity/EntityPhysics.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace basalt::server::runtime {
namespace {

namespace hooks = entity::fishingHooks;
namespace play = wire::protocol::play;
using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;

constexpr std::string_view kHookType = "fishing_bobber";
constexpr std::string_view kRod = "fishing_rod";
constexpr double kParticleRange = 32.0;
constexpr double kHookGravity = 0.03;
constexpr double kHookInertia = 0.92;
/// parity: `FishingHook.retrieve`'s wear.
constexpr std::int32_t kItemPullWear = 3;
constexpr std::int32_t kEntityPullWear = 5;
constexpr std::int32_t kGroundWear = 2;
/// parity: `EntityEvent.FISHING_ROD_REEL_IN`.
constexpr std::int8_t kReelInEvent = 31;
/// parity: the lure and luck enchantments' effect amounts, five seconds and one point a level.
constexpr std::int32_t kLureTicksPerLevel = 100;
constexpr float kHookedEntityHeight = 0.8F;
constexpr double kHitMargin = 0.3;

[[nodiscard]] float soundPitch(random::RandomSource& random) { return 0.4F / ((random.nextFloat() * 0.4F) + 0.8F); }

void playSound(const std::vector<PlayerSession*>& players, std::string_view sound, double x, double y, double z,
               float volume, float pitch, std::int64_t seed) {
    for (const PlayerSession* player : players) {
        player::playSoundFor(*player, sound, play::SoundSource::Neutral, x, y, z, volume, pitch, seed);
    }
}

[[nodiscard]] std::vector<PlayerSession*> playersIn(const player::PlayerRegistry& players,
                                                    const world::LoadedWorld& world) {
    std::vector<PlayerSession*> found;
    for (PlayerSession* player : players.snapshot()) {
        if (player->worldName == world.name()) {
            found.push_back(player);
        }
    }
    return found;
}

[[nodiscard]] bool isRodAt(const PlayerSession& player, std::int32_t slot) {
    const std::optional<storage::StoredItemStack>& stack = player.inventory.at(slot);
    return stack.has_value() && stack->id == "minecraft:fishing_rod";
}

/// parity: `ItemStack.hurtAndBreak` on the rod, which a creative player never feels.
void wearRod(const player::BreakContext& context, PlayerSession& player, std::int32_t slot, std::int32_t amount,
             std::int64_t seed) {
    const std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
    if (amount <= 0 || !held.has_value() || player.gamemode == play::Gamemode::Creative) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(seed);
    const item::ItemDamageEffects effects{
        [&context](const Identifier& id) {
            return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        },
        [&context](const std::string_view tag, const Identifier& item) {
            return context.itemTags != nullptr && world::realItemTags::contains(*context.itemTags, tag, item);
        }};
    const std::optional<storage::StoredItemStack> worn =
        item::itemWear::hurt(*held, amount, random,
                             [&effects](const storage::StoredItemStack& stack, std::int32_t cost,
                                        random::RandomSource& source) { return effects.process(stack, cost, source); });
    if (worn == held) {
        return;
    }
    player.inventory.set(slot, worn);
    if (!worn.has_value()) {
        player::statAwards::itemBroken(player, held->id,
                                       slot == player::PlayerInventory::kOffhandSlot
                                           ? player::statAwards::kBreakOffHand
                                           : player::statAwards::kBreakMainHand,
                                       context.registry);
    }
    if (context.slotChanged) {
        context.slotChanged(player, slot);
    }
}

[[nodiscard]] hooks::WaterLayer layerAt(const world::WorldState& world, std::int32_t x, std::int32_t y,
                                        std::int32_t z) {
    const std::int32_t state = world.blockState(x, y, z).value_or(0);
    if (registry::blocks::isAir(state) || state == registry::blocks::named("lily_pad")->stateId) {
        return hooks::WaterLayer::AboveWater;
    }
    const bool water = world::fluidFlow::kindOf(state) == world::FluidKind::Water;
    return water && world::isSourceFluid(state) && registry::blockShapes::collisionBoxes(state).empty()
               ? hooks::WaterLayer::InsideWater
               : hooks::WaterLayer::Invalid;
}

} // namespace

FishingSystem::FishingSystem(player::PlayerRegistry& players, ExperienceOrbSystem& orbs, Options options)
    : players_(&players), orbs_(&orbs), options_(std::move(options)) {}

bool FishingSystem::isHook(const ServerEntity& entity) { return entity.type().path() == kHookType; }

bool FishingSystem::isRod(std::string_view item) { return item == kRod || item == "minecraft:fishing_rod"; }

bool FishingSystem::useRod(const player::BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                           std::int32_t slot) {
    if (!isRodAt(player, slot)) {
        return false;
    }
    // parity: `FishingRodItem.use` counts the cast and the reel alike.
    player::statAwards::used(player, "minecraft:fishing_rod");
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const std::vector<PlayerSession*> heard = playersIn(*players_, world);
    ServerEntity* hook = player.fishingHook.has_value() ? world.state().entity(*player.fishingHook) : nullptr;
    if (hook != nullptr && !hook->removed && isHook(*hook)) {
        const std::int32_t wear = retrieve(context, player, world, *hook, slot);
        wearRod(context, player, slot, wear, random.nextLong());
        playSound(heard, "entity.fishing_bobber.retrieve", player.x, player.y, player.z, 1.0F, soundPitch(random),
                  random.nextLong());
        world::vibrations::emit(world.state(), "item_interact_finish", player.x, player.y, player.z, world.gameTime(),
                                world::triggerFacts::vibrationSourceOf(player, context.triggers));
        return true;
    }
    playSound(heard, "entity.fishing_bobber.throw", player.x, player.y, player.z, 0.5F, soundPitch(random),
              random.nextLong());
    cast(context, player, world, slot);
    world::vibrations::emit(world.state(), "item_interact_start", player.x, player.y, player.z, world.gameTime(),
                            world::triggerFacts::vibrationSourceOf(player, context.triggers));
    return true;
}

void FishingSystem::cast(const player::BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                         std::int32_t slot) {
    static_cast<void>(context);
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(Identifier::vanilla(std::string{kHookType}));
    const std::optional<storage::StoredItemStack> rod = player.inventory.at(slot);
    if (entry == nullptr) {
        return;
    }
    auto hook = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), entry->id,
                                               entry->protocolId, player.x, player.y, player.z);
    auto state = std::make_shared<hooks::HookState>();
    state->ownerId = player.entityId();
    state->random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    if (rod.has_value()) {
        state->luck = player::enchantmentLevel(*rod, Identifier::vanilla("luck_of_the_sea"));
        state->lureSpeed = player::enchantmentLevel(*rod, Identifier::vanilla("lure")) * kLureTicksPerLevel;
    }
    hooks::cast(*hook, *state, player.x, player.y + world::fluids::eyeHeight(player.sneaking), player.z, player.yaw,
                player.pitch);
    // parity: `getAddEntityPacket` carries the owner.
    hook->objectData = player.entityId();
    hook->ownerId = player.entityId();
    hook->hook = std::move(state);
    player.fishingHook = world.state().spawnEntity(std::move(hook)).entityId();
}

PlayerSession* FishingSystem::ownerOf(const ServerEntity& hook) const {
    if (hook.hook == nullptr) {
        return nullptr;
    }
    for (PlayerSession* player : players_->snapshot()) {
        if (player->entityId() == hook.hook->ownerId) {
            return player;
        }
    }
    return nullptr;
}

void FishingSystem::discard(ServerEntity& hook) {
    hook.removed = true;
    if (PlayerSession* owner = ownerOf(hook); owner != nullptr && owner->fishingHook == hook.entityId()) {
        owner->fishingHook.reset();
    }
}

bool FishingSystem::stopsFishing(world::LoadedWorld& world, ServerEntity& hook, const PlayerSession* owner) {
    if (owner != nullptr && !owner->dead && owner->gamemode != play::Gamemode::Spectator &&
        owner->worldName == world.name()) {
        const bool rodInHand = isRodAt(*owner, player::PlayerInventory::kHotbarStart + owner->inventory.heldSlot()) ||
                               isRodAt(*owner, player::PlayerInventory::kOffhandSlot);
        const double dx = owner->x - hook.x;
        const double dy = owner->y - hook.y;
        const double dz = owner->z - hook.z;
        if (rodInHand && (dx * dx) + (dy * dy) + (dz * dz) <= hooks::kMaxOwnerDistanceSquared) {
            return false;
        }
    }
    discard(hook);
    return true;
}

std::int32_t FishingSystem::retrieve(const player::BreakContext& context, PlayerSession& player,
                                     world::LoadedWorld& world, ServerEntity& hook, std::int32_t slot) {
    hooks::HookState* state = hook.hook.get();
    if (state == nullptr || stopsFishing(world, hook, &player)) {
        return 0;
    }
    std::int32_t wear = 0;
    if (state->hookedIn.has_value()) {
        const double pullX = (player.x - hook.x) * 0.1;
        const double pullY = (player.y - hook.y) * 0.1;
        const double pullZ = (player.z - hook.z) * 0.1;
        bool isItem = false;
        world::TriggerEvent hooked{.entities = {{"entity", world::triggerFacts::ofEntity(world, hook)}}};
        if (const std::optional<world::TriggerItem> rod = world::triggerFacts::itemOf(player.inventory.at(slot));
            rod.has_value()) {
            hooked.items.emplace("rod", *rod);
        }
        if (ServerEntity* caught = world.state().entity(*state->hookedIn); caught != nullptr) {
            hooked.entities.insert_or_assign("entity", world::triggerFacts::ofEntity(world, *caught));
            caught->velocityX += pullX;
            caught->velocityY += pullY;
            caught->velocityZ += pullZ;
        } else if (std::optional<world::DroppedItem> item = world.state().removeItem(*state->hookedIn);
                   item.has_value()) {
            isItem = true;
            hooked.entities.insert_or_assign("entity",
                                             server::world::loot::EntityFacts{.type = Identifier::vanilla("item")});
            hooked.items.insert_or_assign(
                "item", world::TriggerItem{.item = item->item, .count = item->count, .components = item->components});
            item->velocityX += pullX;
            item->velocityY += pullY;
            item->velocityZ += pullZ;
            static_cast<void>(world.state().spawnItem(std::move(*item)));
        }
        // parity: a hooked player pulls itself, when its own client hears the reel-in event.
        for (const PlayerSession* watcher : playersIn(*players_, world)) {
            if (watcher->connection().sendPlayPacket) {
                watcher->connection().sendPlayPacket(play::EntityEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    play::EntityEventPacket{.entityId = hook.entityId(), .eventId = kReelInEvent}.write(buffer);
                });
            }
        }
        if (context.triggers != nullptr) {
            context.triggers->fire("fishing_rod_hooked", player.name(), player.uuid().toString(), hooked);
        }
        wear = isItem ? kItemPullWear : kEntityPullWear;
    } else if (state->nibble > 0) {
        const server::world::loot::LootTable* table =
            context.lootTables == nullptr ? nullptr : context.lootTables->byId(Identifier::vanilla("gameplay/fishing"));
        if (table != nullptr) {
            random::RandomSource scratch = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
            random::RandomSource& roll =
                table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : scratch;
            world::RealLootTables* tables = context.lootTables;
            server::world::loot::LootContext lootContext;
            lootContext.hasEntity = true;
            // parity: `THIS_ENTITY` is the hook itself.
            lootContext.thisEntity =
                server::world::loot::EntityFacts{.type = hook.type(), .scoreName = hook.uuid().toString()};
            lootContext.inOpenWater = state->openWater;
            tables->prepareAt(lootContext, world, hook.x, hook.y, hook.z);
            // parity: `FishingHook.retrieve` rolls with the rod's luck plus the player's own.
            const float luck = static_cast<float>(state->luck) + player.luck();
            for (const server::world::loot::ItemStack& stack : table->generate(roll, luck, std::nullopt, lootContext)) {
                const double xa = player.x - hook.x;
                const double ya = player.y - hook.y;
                const double za = player.z - hook.z;
                world::DroppedItem dropped{
                    .entityId = players_->allocateEntityId(),
                    .uuid = wire::common::Uuid::random(),
                    .item = stack.item,
                    .count = stack.count,
                    .x = hook.x,
                    .y = hook.y,
                    .z = hook.z,
                    .velocityX = xa * 0.1,
                    .velocityY = (ya * 0.1) + (std::sqrt(std::sqrt((xa * xa) + (ya * ya) + (za * za))) * 0.08),
                    .velocityZ = za * 0.1};
                dropped.components = stack.components;
                dropped.pickupDelay = 0;
                static_cast<void>(world.state().spawnItem(std::move(dropped)));
                orbs_->spawn(world, player.x, player.y + 0.5, player.z + 0.5, state->random.nextInt(6) + 1);
                if (context.triggers != nullptr) {
                    world::TriggerEvent caught{.entities = {{"entity", world::triggerFacts::ofEntity(world, hook)}},
                                               .items = {{"item", world::TriggerItem{.item = stack.item,
                                                                                     .count = stack.count,
                                                                                     .components = stack.components}}}};
                    if (const std::optional<world::TriggerItem> rod =
                            world::triggerFacts::itemOf(player.inventory.at(slot));
                        rod.has_value()) {
                        caught.items.emplace("rod", *rod);
                    }
                    context.triggers->fire("fishing_rod_hooked", player.name(), player.uuid().toString(), caught);
                }
                // parity: `Stats.FISH_CAUGHT` counts what the `fishes` tag holds, not every catch.
                if (context.itemTags != nullptr &&
                    world::realItemTags::membersOf(*context.itemTags, "minecraft:fishes").contains(stack.item)) {
                    player.stats.increment(storage::stats::fishCaught());
                }
            }
        }
        wear = 1;
    }
    if (hook.onGround) {
        wear = kGroundWear;
    }
    discard(hook);
    return wear;
}

void FishingSystem::tick(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                         const std::vector<ServerEntity*>& living, ServerEntity& hook) {
    hooks::HookState* state = hook.hook.get();
    const PlayerSession* owner = ownerOf(hook);
    if (state == nullptr || owner == nullptr) {
        discard(hook);
        return;
    }
    if (stopsFishing(world, hook, owner)) {
        return;
    }
    if (hook.onGround) {
        if (++state->life >= hooks::kMaxGroundLife) {
            discard(hook);
            return;
        }
    } else {
        state->life = 0;
    }
    const std::int32_t bx = wire::common::mth::floor(hook.x);
    const std::int32_t by = wire::common::mth::floor(hook.y);
    const std::int32_t bz = wire::common::mth::floor(hook.z);
    const std::int32_t here = world.state().blockState(bx, by, bz).value_or(0);
    const bool waterFluid = world::fluidFlow::kindOf(here) == world::FluidKind::Water;
    const float liquidHeight = waterFluid ? world::fluidFlow::heightAt(world.state(), bx, by, bz) : 0.0F;
    const bool inWater = liquidHeight > 0.0F;
    if (state->phase == hooks::Phase::Flying) {
        if (state->hookedIn.has_value()) {
            hook.velocityX = hook.velocityY = hook.velocityZ = 0.0;
            state->phase = hooks::Phase::HookedInEntity;
            return;
        }
        if (inWater) {
            hook.velocityX *= 0.3;
            hook.velocityY *= 0.2;
            hook.velocityZ *= 0.3;
            state->phase = hooks::Phase::Bobbing;
            return;
        }
        hookFirstHit(world, watchers, living, hook, *state);
    } else {
        if (state->phase == hooks::Phase::HookedInEntity) {
            if (state->hookedIn.has_value()) {
                std::optional<std::array<double, 4>> at;
                if (const ServerEntity* caught = world.state().entity(*state->hookedIn);
                    caught != nullptr && !caught->removed && !caught->dying) {
                    at = std::array{caught->x, caught->y, caught->z, caught->height()};
                }
                for (const PlayerSession* player : watchers) {
                    if (player->entityId() == *state->hookedIn && !player->dead) {
                        at = std::array{player->x, player->y, player->z, 1.8};
                    }
                }
                for (const world::DroppedItem& item : world.state().items()) {
                    if (item.entityId == *state->hookedIn) {
                        at = std::array{item.x, item.y, item.z, 0.25};
                    }
                }
                if (at.has_value()) {
                    hook.x = (*at)[0];
                    hook.y = (*at)[1] + ((*at)[3] * static_cast<double>(kHookedEntityHeight));
                    hook.z = (*at)[2];
                } else {
                    state->hookedIn.reset();
                    state->phase = hooks::Phase::Flying;
                }
            }
            return;
        }
        bob(world, watchers, hook, *state, liquidHeight, inWater);
    }
    const bool falls = !waterFluid && !hook.onGround && !state->hookedIn.has_value();
    const double wasX = hook.velocityX;
    const double wasZ = hook.velocityZ;
    entity::entityPhysics::move(world.state(), hook, falls ? kHookGravity : 0.0, kHookInertia);
    const bool blockedSideways = (wasX != 0.0 && hook.velocityX == 0.0) || (wasZ != 0.0 && hook.velocityZ == 0.0);
    if (state->phase == hooks::Phase::Flying && (hook.onGround || blockedSideways)) {
        hook.velocityX = hook.velocityY = hook.velocityZ = 0.0;
    }
}

void FishingSystem::bob(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers, ServerEntity& hook,
                        hooks::HookState& state, float liquidHeight, bool inWater) {
    const std::int32_t bx = wire::common::mth::floor(hook.x);
    const std::int32_t by = wire::common::mth::floor(hook.y);
    const std::int32_t bz = wire::common::mth::floor(hook.z);
    double force = hook.y + hook.velocityY - static_cast<double>(by) - static_cast<double>(liquidHeight);
    if (std::abs(force) < 0.01) {
        force += (force > 0.0 ? 1.0 : (force < 0.0 ? -1.0 : 0.0)) * 0.1;
    }
    hook.velocityX *= 0.9;
    hook.velocityY -= force * static_cast<double>(state.random.nextFloat()) * 0.2;
    hook.velocityZ *= 0.9;
    if (state.nibble <= 0 && state.timeUntilHooked <= 0) {
        state.openWater = true;
    } else {
        state.openWater =
            state.openWater && state.outOfWaterTime < 10 &&
            hooks::calculateOpenWater(
                bx, by, bz,
                [&world](std::int32_t x, std::int32_t y, std::int32_t z) { return layerAt(world.state(), x, y, z); });
    }
    if (!inWater) {
        state.outOfWaterTime = std::min(10, state.outOfWaterTime + 1);
        return;
    }
    state.outOfWaterTime = std::max(0, state.outOfWaterTime - 1);
    if (state.biting) {
        // parity: `syncronizedRandom`, reseeded every tick from the uuid and the clock.
        random::RandomSource synced =
            random::RandomSource::legacy(static_cast<std::int64_t>(hook.uuid().low()) ^ world.gameTime());
        const float first = synced.nextFloat();
        hook.velocityY += -0.1 * static_cast<double>(first) * static_cast<double>(synced.nextFloat());
    }
    const std::int32_t water = registry::blocks::named("water")->stateId;
    const server::world::Chunk* column = world.state().chunkIfLoaded(bx >> 4, bz >> 4);
    hooks::Surroundings around;
    around.rainingAbove = world.weather().raining && column != nullptr &&
                          column->heightAt(bx & 15, bz & 15, server::world::HeightmapType::MotionBlocking) <= by + 1;
    around.skyAbove = world::naturalSpawner::canSeeSky(world, bx, by + 1, bz);
    around.isWater = [&world, water](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::int32_t state = world.state().blockState(x, y, z).value_or(0);
        return state >= water && state < water + 16;
    };
    around.particles = [&watchers](std::string_view name, double x, double y, double z, std::int32_t count, double dx,
                                   double dy, double dz, double speed) {
        const registry::ParticleType* type = registry::particleTypes::of(Identifier::vanilla(std::string{name}));
        if (type == nullptr) {
            return;
        }
        const play::LevelParticlesPacket packet{.particleTypeId = type->protocolId,
                                                .x = x,
                                                .y = y,
                                                .z = z,
                                                .offsetX = static_cast<float>(dx),
                                                .offsetY = static_cast<float>(dy),
                                                .offsetZ = static_cast<float>(dz),
                                                .maxSpeedX = static_cast<float>(speed),
                                                .maxSpeedY = static_cast<float>(speed),
                                                .maxSpeedZ = static_cast<float>(speed),
                                                .count = count};
        const std::string full = type->id.toString();
        for (const PlayerSession* player : watchers) {
            const double px = player->x - x;
            const double py = player->y - y;
            const double pz = player->z - z;
            if ((px * px) + (py * py) + (pz * pz) < kParticleRange * kParticleRange) {
                player::showParticleFor(*player, full, packet);
            }
        }
    };
    around.splash = [&watchers, &hook, this](float volume, float pitch) {
        playSound(watchers, "entity.fishing_bobber.splash", hook.x, hook.y, hook.z, volume, pitch,
                  options_.nextSeed ? options_.nextSeed() : 0);
    };
    hooks::catchingFish(hook, state, around);
}

void FishingSystem::hookFirstHit(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 const std::vector<ServerEntity*>& living, ServerEntity& hook,
                                 hooks::HookState& state) {
    // parity: `ProjectileUtil.getEntityHitResult`.
    // by the default margin.
    const double toX = hook.x + hook.velocityX;
    const double toY = hook.y + hook.velocityY;
    const double toZ = hook.z + hook.velocityZ;
    double nearest = 2.0;
    std::optional<std::int32_t> found;
    const auto consider = [&](std::int32_t id, double x, double y, double z, double width, double height) {
        const double half = (width / 2.0) + kHitMargin;
        const std::optional<double> along =
            entity::projectiles::clipBox(hook.x, hook.y, hook.z, toX, toY, toZ, x - half, y - kHitMargin, z - half,
                                         x + half, y + height + kHitMargin, z + half);
        if (along.has_value() && *along < nearest) {
            nearest = *along;
            found = id;
        }
    };
    for (const ServerEntity* mob : living) {
        if (!mob->removed && !mob->dying && registry::entityAttributes::isLiving(mob->type())) {
            consider(mob->entityId(), mob->x, mob->y, mob->z, mob->width(), mob->height());
        }
    }
    for (const PlayerSession* player : watchers) {
        if (player->entityId() != state.ownerId && !player->dead && player->gamemode != play::Gamemode::Spectator) {
            consider(player->entityId(), player->x, player->y, player->z, 0.6, 1.8);
        }
    }
    // parity: `FishingHook.canHitEntity` adds items to what a projectile may hit.
    for (const world::DroppedItem& item : world.state().items()) {
        consider(item.entityId, item.x, item.y, item.z, 0.25, 0.25);
    }
    if (found.has_value()) {
        state.hookedIn = found;
    }
}

} // namespace basalt::server::runtime
