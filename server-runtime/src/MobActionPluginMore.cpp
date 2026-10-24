#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PotionEffects.hpp>

#include <cmath>
#include <memory>

namespace basalt::server::runtime {

using entity::ServerEntity;
namespace equipment = entity::mobEquipment;

namespace {

constexpr std::int32_t kPotionUseTicks = 32;
constexpr std::int32_t kSheepBlue = 11;
constexpr std::int32_t kSheepRed = 14;
constexpr std::string_view kDrinkingModifier = "minecraft:drinking";

[[nodiscard]] server::world::loot::ItemStack potionOf(std::string_view potion) {
    wire::nbt::NbtCompound contents;
    contents.putString("potion", "minecraft:" + std::string{potion});
    wire::nbt::NbtCompound components;
    components.set("minecraft:potion_contents", wire::nbt::NbtTag::ofCompound(std::move(contents)));
    return server::world::loot::ItemStack{
        .item = wire::common::Identifier::vanilla("potion"), .count = 1, .components = std::move(components)};
}

} // namespace

std::int32_t MobActionSystem::pluginRetype(ServerEntity& mob, const wire::common::Identifier& into) {
    world::LoadedWorld* world = worldOf(mob);
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(into);
    if (world == nullptr || entry == nullptr || mob.removed) {
        return -1;
    }
    wire::nbt::NbtCompound tag = entity::entityNbt::encode(mob);
    tag.putString("id", into.toString());
    std::unique_ptr<ServerEntity> converted =
        entity::entityNbt::decode(tag, players_->allocateEntityId(), entry->protocolId);
    if (converted == nullptr) {
        return -1;
    }
    converted->persistent = mob.persistent;
    converted->sourceNbt = tag;
    carryOver(*world, watchersOf(*world), mob, *converted);
    mob.removed = true;
    return world->state().spawnEntity(std::move(converted)).entityId();
}

bool MobActionSystem::wololo(ServerEntity& evoker, std::int32_t sheepId) {
    world::LoadedWorld* world = worldOf(evoker);
    ServerEntity* sheep = world == nullptr ? nullptr : world->state().entity(sheepId);
    if (sheep == nullptr || sheep->removed || sheep->health <= 0.0 || sheep->type().path() != "sheep") {
        return false;
    }
    // parity: `EvokerWololoSpellGoal.performSpellCasting`, a sheep turned red.
    if (!sheep->sourceNbt.has_value()) {
        sheep->sourceNbt = wire::nbt::NbtCompound{};
    }
    sheep->sourceNbt->putByte("Color", static_cast<std::int8_t>(kSheepRed));
    ++sheep->sourceNbtVersion;
    return true;
}

bool MobActionSystem::mobGriefing(const ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    return world == nullptr || world->gameRules().get(world::gameRules::kMobGriefing);
}

bool MobActionSystem::pluginWitchDrink(ServerEntity& witch,
                                       const std::optional<server::world::loot::ItemStack>& potion) {
    world::LoadedWorld* world = worldOf(witch);
    if (world == nullptr || witch.type().path() != "witch") {
        return false;
    }
    entity::mobMisc::State& misc = entity::mobMisc::of(witch);
    if (!potion.has_value()) {
        equipment::set(witch, equipment::Slot::MainHand, std::nullopt, false);
        equipmentChanged(witch, static_cast<std::uint8_t>(equipment::Slot::MainHand));
        misc.witchDrinking = false;
        witch.attributeModifiers[std::string{entity::attribute::kMovementSpeed}].erase(std::string{kDrinkingModifier});
        return true;
    }
    // parity: `Witch.setDrinkingPotion`.
    equipment::set(witch, equipment::Slot::MainHand, potion, false);
    equipmentChanged(witch, static_cast<std::uint8_t>(equipment::Slot::MainHand));
    misc.witchDrinkTicks = kPotionUseTicks;
    misc.witchDrinking = true;
    if (!witch.sourceNbt.has_value() || !witch.sourceNbt->getBoolean("Silent")) {
        random::RandomSource voice = freshRandom();
        playSound(*world, witch, "entity.witch.drink", 1.0F, 0.8F + (voice.nextFloat() * 0.4F));
    }
    witch.attributeModifiers[std::string{entity::attribute::kMovementSpeed}].insert_or_assign(
        std::string{kDrinkingModifier}, entity::AttributeModifier{.id = std::string{kDrinkingModifier},
                                                                  .amount = -0.25,
                                                                  .operation = entity::AttributeOperation::AddValue});
    return true;
}

void MobActionSystem::tickWitchDrinking(world::LoadedWorld& world, ServerEntity& witch, random::RandomSource& random) {
    if (witch.removed || witch.dying || witch.health <= 0.0) {
        return;
    }
    entity::mobMisc::State& misc = entity::mobMisc::of(witch);
    if (misc.witchDrinking) {
        if (misc.witchDrinkTicks-- > 0) {
            return;
        }
        misc.witchDrinking = false;
        const std::optional<server::world::loot::ItemStack> held = equipment::get(witch, equipment::Slot::MainHand);
        equipment::set(witch, equipment::Slot::MainHand, std::nullopt, false);
        equipmentChanged(witch, static_cast<std::uint8_t>(equipment::Slot::MainHand));
        if (held.has_value() && held->item.path() == "potion") {
            if (const wire::nbt::NbtCompound* contents = held->components.getCompound("minecraft:potion_contents");
                contents != nullptr) {
                for (const world::ActiveEffect& effect : world::potionEffects::of(*contents, 1.0F)) {
                    static_cast<void>(addMobEffect(world, watchersOf(world), witch, effect, witch.entityId()));
                }
            }
        }
        witch.attributeModifiers[std::string{entity::attribute::kMovementSpeed}].erase(std::string{kDrinkingModifier});
        return;
    }
    const auto hasEffect = [&witch](std::string_view name) {
        return witch.effects.contains(wire::common::Identifier::vanilla(std::string{name}));
    };
    const std::optional<std::int32_t> eyeState = world.state().blockState(
        static_cast<std::int32_t>(std::floor(witch.x)), static_cast<std::int32_t>(std::floor(witch.y + 1.62)),
        static_cast<std::int32_t>(std::floor(witch.z)));
    const bool eyeInWater = eyeState.has_value() && world::fluids::of(*eyeState) == world::Submersion::Water;
    std::string_view potion;
    if (random.nextFloat() < 0.15F && eyeInWater && !hasEffect("water_breathing")) {
        potion = "water_breathing";
    } else if (random.nextFloat() < 0.15F && witch.fireTicks > 0 && !hasEffect("fire_resistance")) {
        potion = "fire_resistance";
    } else if (random.nextFloat() < 0.05F && witch.health < witch.attributeWithModifiers("minecraft:max_health")) {
        potion = "healing";
    } else if (random.nextFloat() < 0.5F && witch.targetId.has_value() && !hasEffect("speed")) {
        const ServerEntity* target = world.state().entity(*witch.targetId);
        double distanceSquared = 0.0;
        if (target != nullptr) {
            const double dx = target->x - witch.x;
            const double dy = target->y - witch.y;
            const double dz = target->z - witch.z;
            distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        } else {
            for (player::PlayerSession* session : watchersOf(world)) {
                if (session->entityId() == *witch.targetId) {
                    const double dx = session->x - witch.x;
                    const double dy = session->y - witch.y;
                    const double dz = session->z - witch.z;
                    distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
                }
            }
        }
        if (distanceSquared > 121.0) {
            potion = "swiftness";
        }
    }
    if (!potion.empty()) {
        static_cast<void>(pluginWitchDrink(witch, potionOf(potion)));
    }
    if (random.nextFloat() < 7.5E-4F) {
        entityEvent(witch, 15);
    }
}

void MobActionSystem::tickStriderWarmth(world::LoadedWorld& world, ServerEntity& strider) {
    if (strider.noAi) {
        return;
    }
    // parity: `Strider.tick`, cold anywhere but in or over lava.
    const auto warmAt = [&](double y) {
        const std::optional<std::int32_t> state = world.state().blockState(
            static_cast<std::int32_t>(std::floor(strider.x)), static_cast<std::int32_t>(std::floor(y)),
            static_cast<std::int32_t>(std::floor(strider.z)));
        const wire::common::Identifier* block =
            state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
        return block != nullptr && blockTagged("minecraft:strider_warm_blocks", *block).value_or(false);
    };
    entity::mobMisc::of(strider).striderCold = !(warmAt(strider.y) || warmAt(strider.y - 0.2));
}

bool MobActionSystem::pluginSulfurSwallow(ServerEntity& cube, const server::world::loot::ItemStack& item) {
    // parity: `SulfurCube.canHoldItem`, a grown cube with nothing in it taking something it can swallow.
    if (cube.type().path() != "sulfur_cube" || cube.age < 0 || cube.removed || cube.dying ||
        equipment::get(cube, equipment::Slot::Body).has_value() ||
        !itemTagged("minecraft:sulfur_cube_swallowable", item.item)) {
        return false;
    }
    server::world::loot::ItemStack one = item;
    one.count = 1;
    equipment::set(cube, equipment::Slot::Body, one, true);
    equipmentChanged(cube, static_cast<std::uint8_t>(equipment::Slot::Body));
    return true;
}

} // namespace basalt::server::runtime
