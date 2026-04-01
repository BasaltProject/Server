#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/villager/Gossips.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::SetEntityDataPacket;
namespace conversions = entity::mobConversions;
namespace mth = wire::common::mth;

constexpr double kLevelEventRange = 64.0;
/// parity: `Entity.thunderHit`'s five points and eight seconds.
constexpr float kLightningDamage = 5.0F;
constexpr std::int32_t kLightningFireTicks = 160;
/// parity: `MushroomCow.DATA_TYPE` after `AgeableMob`'s two, and `Creeper.DATA_IS_POWERED` after
/// its swell direction.
constexpr std::uint8_t kMooshroomTypeIndex = 18;
constexpr std::uint8_t kCreeperPoweredIndex = 17;
/// parity: `LevelEvent.SOUND_ZOMBIE_INFECTED`.
constexpr std::int32_t kInfectionEvent = 1026;

[[nodiscard]] bool isBlock(const world::LoadedWorld& world, double x, double y, double z, std::string_view path) {
    const std::optional<std::int32_t> state = world.state().blockState(mth::floor(x), mth::floor(y), mth::floor(z));
    const wire::common::Identifier* block =
        state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && block->path() == path;
}

void send(const std::vector<PlayerSession*>& watchers, const SetEntityDataPacket& packet) {
    for (PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

} // namespace

bool MobActionSystem::tickConversion(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                     ServerEntity& mob) {
    const bool wasShaking = conversions::isShaking(mob);
    const double eyeY = mob.y + entity::mobBreathing::eyeHeightOf(mob);
    const std::int32_t eyeBlockY = mth::floor(eyeY);
    const std::optional<std::int32_t> eyeState =
        world.state().blockState(mth::floor(mob.x), eyeBlockY, mth::floor(mob.z));
    const bool eyeInWater = eyeState.has_value() && world::fluidFlow::kindOf(*eyeState) == world::FluidKind::Water &&
                            eyeY < eyeBlockY + static_cast<double>(world::fluidFlow::heightAt(
                                                   world.state(), mth::floor(mob.x), eyeBlockY, mth::floor(mob.z)));
    const conversions::Around around{
        .eyeInWater = eyeInWater,
        .inPowderSnow =
            isBlock(world, mob.x, mob.y, mob.z, "powder_snow") || isBlock(world, mob.x, eyeY, mob.z, "powder_snow"),
        // parity: `gameplay/piglins_zombify`, which only the Nether's dimension type turns off.
        .piglinsZombify = world.declaration().dimension != config::WorldDimension::Nether,
        .cureProgress =
            mob.conversionCountdown >= 0 && mob.type().path() == "zombie_villager" ? cureProgress(world, mob) : 1};
    const std::optional<conversions::Conversion> due = conversions::tick(mob, around);
    if (const std::optional<std::uint8_t> index = conversions::shakingIndexOf(mob.type().path());
        index.has_value() && conversions::isShaking(mob) != wasShaking) {
        send(watchers,
             SetEntityDataPacket{
                 .entityId = mob.entityId(),
                 .entries = {SetEntityDataPacket::Entry{
                     .index = *index, .value = SetEntityDataPacket::BoolValue{.value = conversions::isShaking(mob)}}}});
    }
    if (!due.has_value()) {
        return false;
    }
    if (mob.type().path() != "zombie_villager") {
        return convertMob(world, watchers, mob, *due, {});
    }
    // parity: `ZombieVillager.finishConversion` hands the villager back everything it was.
    const wire::nbt::NbtCompound zombieData = entity::entityNbt::encode(mob);
    const std::span<const std::int32_t> starter =
        mob.sourceNbt.has_value() ? mob.sourceNbt->getIntArray("ConversionPlayer") : std::span<const std::int32_t>{};
    std::optional<std::string> curer;
    if (starter.size() == 4) {
        const auto half = [&starter](std::size_t at) {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(starter[at])) << 32U) |
                   static_cast<std::uint32_t>(starter[at + 1]);
        };
        const wire::common::Uuid uuid{half(0), half(2)};
        if (players_->get(uuid) != nullptr) {
            curer = uuid.toString();
        }
    }
    return convertMob(world, watchers, mob, *due, [&zombieData, &curer](wire::nbt::NbtCompound& tag) {
        for (const std::string_view key : {"VillagerData", "Offers", "Gossips", "Xp", "VillagerDataFinalized"}) {
            if (const wire::nbt::NbtTag* value = zombieData.find(key)) {
                tag.set(key, *value);
            }
        }
        // parity: `ReputationEventType.ZOMBIE_VILLAGER_CURED`, for the curer if they are still here.
        if (curer.has_value()) {
            entity::villager::Gossips gossips;
            if (const wire::nbt::NbtList* old = tag.getList("Gossips"); old != nullptr) {
                gossips.decode(*old);
            }
            gossips.add(*curer, entity::villager::GossipType::kMajorPositive,
                        entity::villager::Gossips::kPerEverlastingMemory);
            gossips.add(*curer, entity::villager::GossipType::kMinorPositive, entity::villager::Gossips::kPerEvent);
            tag.set("Gossips", wire::nbt::NbtTag::ofList(gossips.encode()));
        }
    });
}

std::int32_t MobActionSystem::cureProgress(world::LoadedWorld& world, const ServerEntity& mob) {
    // parity: `ZombieVillager.getConversionProgress`: one in a hundred ticks it looks for iron bars
    // and beds around it, and each one found may add a tick.
    random::RandomSource random = freshRandom();
    std::int32_t amount = 1;
    if (random.nextFloat() >= 0.01F) {
        return amount;
    }
    std::int32_t found = 0;
    const auto x0 = static_cast<std::int32_t>(mob.x);
    const auto y0 = static_cast<std::int32_t>(mob.y);
    const auto z0 = static_cast<std::int32_t>(mob.z);
    for (std::int32_t x = x0 - 4; x < x0 + 4 && found < 14; ++x) {
        for (std::int32_t y = y0 - 4; y < y0 + 4 && found < 14; ++y) {
            for (std::int32_t z = z0 - 4; z < z0 + 4 && found < 14; ++z) {
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                const wire::common::Identifier* block =
                    state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
                if (block == nullptr || (block->path() != "iron_bars" && !block->path().ends_with("_bed"))) {
                    continue;
                }
                if (random.nextFloat() < 0.3F) {
                    amount++;
                }
                found++;
            }
        }
    }
    return amount;
}

bool MobActionSystem::startCure(world::LoadedWorld& world, ServerEntity& mob, const wire::common::Uuid& player) {
    // parity: `ZombieVillager.mobInteract`: a golden apple only works on a weakened one.
    if (mob.type().path() != "zombie_villager" ||
        !entity::mobStatusEffects::has(mob, wire::common::Identifier::vanilla("weakness"))) {
        return false;
    }
    random::RandomSource random = freshRandom();
    const std::int32_t time = random.nextInt(conversions::kCureSpread) + conversions::kCureMinTicks;
    mob.conversionCountdown = time;
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    wire::nbt::NbtCompound data = mob.sourceNbt.value_or(wire::nbt::NbtCompound{});
    data.set("ConversionPlayer",
             wire::nbt::NbtTag::ofIntArray({static_cast<std::int32_t>(static_cast<std::uint32_t>(player.high() >> 32)),
                                            static_cast<std::int32_t>(static_cast<std::uint32_t>(player.high())),
                                            static_cast<std::int32_t>(static_cast<std::uint32_t>(player.low() >> 32)),
                                            static_cast<std::int32_t>(static_cast<std::uint32_t>(player.low()))}));
    mob.sourceNbt = std::move(data);
    static_cast<void>(entity::mobStatusEffects::remove(mob, wire::common::Identifier::vanilla("weakness")));
    // parity: `Math.min(difficulty - 1, 0)`, which is never above zero.
    const std::int32_t amplifier = world.difficulty() == "peaceful" ? -1 : 0;
    static_cast<void>(
        entity::mobStatusEffects::add(mob, world::ActiveEffect{.effect = wire::common::Identifier::vanilla("strength"),
                                                               .amplifier = amplifier,
                                                               .remainingTicks = time}));
    send(watchers, SetEntityDataPacket{.entityId = mob.entityId(),
                                       .entries = {SetEntityDataPacket::Entry{
                                           .index = 19, .value = SetEntityDataPacket::BoolValue{.value = true}}}});
    // parity: entity event 16, which the client hears as the cure starting.
    for (PlayerSession* watcher : watchers) {
        if (watcher->connection().sendPlayPacket) {
            watcher->connection().sendPlayPacket(
                wire::protocol::play::EntityEventPacket::kId, [&mob](wire::io::ByteBuffer& buffer) {
                    wire::protocol::play::EntityEventPacket{.entityId = mob.entityId(), .eventId = 16}.write(buffer);
                });
        }
    }
    return true;
}

bool MobActionSystem::convertMob(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& mob, const entity::mobConversions::Conversion& conversion,
                                 const std::function<void(wire::nbt::NbtCompound& converted)>& finish) {
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(conversion.into);
    if (entry == nullptr || mob.removed) {
        return false;
    }
    wire::nbt::NbtCompound tag = conversions::convertedTag(entity::entityNbt::encode(mob), mob.type(), conversion);
    if (finish) {
        finish(tag);
    }
    std::unique_ptr<ServerEntity> converted =
        entity::entityNbt::decode(tag, players_->allocateEntityId(), entry->protocolId);
    if (converted == nullptr) {
        return false;
    }
    converted->persistent = converted->persistent || conversion.persistent;
    converted->sourceNbt = tag;
    // parity: the nausea a zombified piglin or a zoglin wakes up with.
    if (conversion.nausea) {
        static_cast<void>(entity::mobStatusEffects::add(
            *converted,
            world::ActiveEffect{.effect = wire::common::Identifier::vanilla("nausea"), .remainingTicks = 200}));
    }
    const bool silent = mob.sourceNbt.has_value() && mob.sourceNbt->getBoolean("Silent");
    // parity: `SINGLE` discards the old mob; it neither dies nor drops anything.
    mob.removed = true;
    static_cast<void>(world.state().spawnEntity(std::move(converted)));
    if (conversion.levelEvent != 0 && !silent) {
        const std::int32_t x = mth::floor(mob.x);
        const std::int32_t y = mth::floor(mob.y);
        const std::int32_t z = mth::floor(mob.z);
        for (PlayerSession* player : watchers) {
            const double dx = player->x - x;
            const double dy = player->y - y;
            const double dz = player->z - z;
            if ((dx * dx) + (dy * dy) + (dz * dz) <= kLevelEventRange * kLevelEventRange &&
                player->connection().sendLevelEvent) {
                player->connection().sendLevelEvent(conversion.levelEvent, x, y, z, 0);
            }
        }
    }
    return true;
}

bool MobActionSystem::infectVillager(world::LoadedWorld& world, const ServerEntity& zombie, ServerEntity& villager) {
    // parity: `Zombie.killedEntity`, which every zombie kind inherits: sure on hard, a coin flip on
    // normal, never on easy.
    static constexpr std::array kZombies{"zombie", "husk", "drowned", "zombie_villager", "zombified_piglin"};
    const std::string_view difficulty = world.difficulty();
    if (villager.type().path() != "villager" || std::ranges::find(kZombies, zombie.type().path()) == kZombies.end() ||
        (difficulty != "normal" && difficulty != "hard")) {
        return false;
    }
    random::RandomSource random = freshRandom();
    if (difficulty != "hard" && random.nextBoolean()) {
        return false;
    }
    const bool silent = zombie.sourceNbt.has_value() && zombie.sourceNbt->getBoolean("Silent");
    const wire::nbt::NbtCompound villagerData = entity::entityNbt::encode(villager);
    const conversions::Conversion conversion{.into = wire::common::Identifier::vanilla("zombie_villager"),
                                             .levelEvent = silent ? 0 : kInfectionEvent};
    const bool converted =
        convertMob(world, watchersOf(world), villager, conversion, [&villagerData](wire::nbt::NbtCompound& tag) {
            for (const std::string_view key : {"VillagerData", "Offers", "Gossips", "Xp"}) {
                if (const wire::nbt::NbtTag* value = villagerData.find(key)) {
                    tag.set(key, *value);
                }
            }
            tag.putBoolean("VillagerDataFinalized", true);
        });
    if (converted) {
        // parity: `Villager.die` has already run by the time `Zombie.killedEntity` converts it.
        villagerDied(world, villager, zombie.uuid().toString());
        // parity: `LivingEntity.die` skips the loot and the death once the killer answers false.
        villager.killed = false;
        villager.dying = false;
    }
    return converted;
}

bool MobActionSystem::addMobEffect(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                   ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId) {
    if (mob.removed || mob.dying || !entity::mobStatusEffects::canBeAffected(mob, effect.effect)) {
        return false;
    }
    const entity::mobStatusEffects::Instant instant =
        entity::mobStatusEffects::instantOf(mob, effect.effect, effect.amplifier);
    if (instant.heal > 0.0F) {
        mob.health = std::min(mob.attributeWithModifiers("minecraft:max_health"),
                              mob.health + static_cast<double>(instant.heal));
        return true;
    }
    if (instant.damage > 0.0F) {
        const entity::entityCombat::Result result = entity::entityCombat::hurt(
            mob, instant.damage, std::nullopt, std::nullopt, hooks_.combat, sourceId >= 0 ? "indirect_magic" : "magic");
        if (result.landed) {
            combat_->announceEntityHit(watchers, mob, world::damageTypes::kMagic, sourceId, result.died);
        }
        return true;
    }
    static_cast<void>(world);
    return entity::mobStatusEffects::add(mob, effect);
}

void MobActionSystem::tickMobEffects(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                     ServerEntity& mob) {
    if (mob.effects.empty()) {
        return;
    }
    std::vector<wire::common::Identifier> expired;
    for (const entity::mobStatusEffects::TickEffect& owed : entity::mobStatusEffects::tick(mob, expired)) {
        if (owed.heal > 0.0F) {
            mob.health = std::min(mob.attributeWithModifiers("minecraft:max_health"),
                                  mob.health + static_cast<double>(owed.heal));
            continue;
        }
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(mob, owed.damage, std::nullopt, std::nullopt, hooks_.combat, owed.cause);
        if (result.landed) {
            combat_->announceEntityHit(
                watchers, mob, owed.cause == "wither" ? world::damageTypes::kWither : world::damageTypes::kMagic, -1,
                result.died);
        }
    }
    // parity: `LivingEntity.onAttributeUpdated`, which takes back health a lost boost held up.
    if (!expired.empty()) {
        mob.health = std::min(mob.health, mob.attributeWithModifiers("minecraft:max_health"));
    }
    static_cast<void>(world);
}

void MobActionSystem::thunderHit(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& mob, std::int32_t boltId) {
    const std::string_view path = mob.type().path();
    const bool peaceful = world.difficulty() == "peaceful";
    // parity: `ArmorStand` and `BlockAttachedEntity` override it with nothing.
    if (path == "armor_stand" || path == "item_frame" || path == "glow_item_frame" || path == "painting" ||
        path == "leash_knot") {
        return;
    }
    // parity: `Pig.thunderHit`, a zombified piglin with a fresh golden weapon and no despawning.
    if (path == "pig" && !peaceful) {
        random::RandomSource random = freshRandom();
        const conversions::Conversion conversion{.into = wire::common::Identifier::vanilla("zombified_piglin"),
                                                 .keepEquipment = false};
        if (convertMob(world, watchers, mob, conversion, [&random](wire::nbt::NbtCompound& tag) {
                wire::nbt::NbtCompound stack;
                stack.putString("id", random.nextInt(20) == 0 ? "minecraft:golden_spear" : "minecraft:golden_sword");
                stack.putInt("count", 1);
                wire::nbt::NbtCompound equipment;
                equipment.set("mainhand", wire::nbt::NbtTag::ofCompound(std::move(stack)));
                tag.set("equipment", wire::nbt::NbtTag::ofCompound(std::move(equipment)));
                tag.putBoolean("PersistenceRequired", true);
            })) {
            return;
        }
    }
    // parity: `Villager.thunderHit`, a witch that stays.
    if (path == "villager" && !peaceful) {
        const conversions::Conversion conversion{
            .into = wire::common::Identifier::vanilla("witch"), .keepEquipment = false, .keepCanPickUpLoot = false};
        if (convertMob(world, watchers, mob, conversion,
                       [](wire::nbt::NbtCompound& tag) { tag.putBoolean("PersistenceRequired", true); })) {
            releaseVillagerPois(world, mob);
            return;
        }
    }
    // parity: `MushroomCow.thunderHit`, once per bolt and without being hurt.
    if (path == "mooshroom") {
        if (mob.lastLightningBolt != boltId) {
            mob.lastLightningBolt = boltId;
            wire::nbt::NbtCompound data = mob.sourceNbt.value_or(wire::nbt::NbtCompound{});
            const bool brown = data.getString("Type") == "brown";
            data.putString("Type", brown ? "red" : "brown");
            mob.sourceNbt = std::move(data);
            send(watchers, SetEntityDataPacket{.entityId = mob.entityId(),
                                               .entries = {SetEntityDataPacket::Entry{
                                                   .index = kMooshroomTypeIndex,
                                                   .value = SetEntityDataPacket::IntValue{.value = brown ? 0 : 1}}}});
            playSound(world, mob, "entity.mooshroom.convert", 2.0F, 1.0F);
        }
        return;
    }
    // parity: `Entity.thunderHit`: a tick more of fire, eight seconds of it when it was not burning,
    // and five points. A turtle takes everything. Not burning is vanilla's minus one, our zero.
    mob.fireTicks = mob.fireTicks <= 0 ? kLightningFireTicks : mob.fireTicks + 1;
    const float damage = path == "turtle" ? std::numeric_limits<float>::max() : kLightningDamage;
    const entity::entityCombat::Result result =
        entity::entityCombat::hurt(mob, damage, std::nullopt, std::nullopt, hooks_.combat, "lightning_bolt");
    if (result.landed) {
        combat_->announceEntityHit(watchers, mob, world::damageTypes::kLightningBolt, boltId, result.died);
    }
    if (entity::copperGolems::isCopperGolem(mob.type())) {
        golemStruckByLightning(mob, boltId);
    }
    // parity: `Creeper.thunderHit`, which charges it after the hit.
    if (path == "creeper" && !mob.powered) {
        mob.powered = true;
        send(watchers, SetEntityDataPacket{.entityId = mob.entityId(),
                                           .entries = {SetEntityDataPacket::Entry{
                                               .index = kCreeperPoweredIndex,
                                               .value = SetEntityDataPacket::BoolValue{.value = true}}}});
    }
}

} // namespace basalt::server::runtime
