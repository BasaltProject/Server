#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <array>
#include <string>

namespace basalt::server::runtime {

using entity::ServerEntity;
using server::world::loot::ItemStack;
namespace equipment = entity::mobEquipment;

namespace {

constexpr std::array<std::string_view, 16> kDyes{"white", "orange", "magenta", "light_blue", "yellow", "lime",
                                                 "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                 "brown", "green",  "red",        "black"};

[[nodiscard]] bool sourceFlag(const ServerEntity& mob, std::string_view key, bool fallback) {
    return mob.sourceNbt.has_value() ? mob.sourceNbt->getBoolean(key, fallback) : fallback;
}

void putSource(ServerEntity& mob, std::string_view key, bool value) {
    if (!mob.sourceNbt.has_value()) {
        mob.sourceNbt = wire::nbt::NbtCompound{};
    }
    mob.sourceNbt->putBoolean(key, value);
    ++mob.sourceNbtVersion;
}

} // namespace

bool MobActionSystem::readyForShearing(const ServerEntity& mob) const {
    const std::string_view path = mob.type().path();
    if (path == "sheep") {
        return !sourceFlag(mob, "Sheared", false) && mob.age >= 0;
    }
    if (path == "mooshroom") {
        return mob.age >= 0;
    }
    if (path == "snow_golem") {
        return sourceFlag(mob, "Pumpkin", true);
    }
    if (path == "bogged") {
        return !sourceFlag(mob, "sheared", false);
    }
    if (path == "copper_golem") {
        const std::optional<ItemStack> worn = equipment::get(mob, equipment::Slot::Saddle);
        return worn.has_value() && itemTagged("minecraft:shearable_from_copper_golem", worn->item);
    }
    if (path == "sulfur_cube") {
        return equipment::get(mob, equipment::Slot::Body).has_value();
    }
    return false;
}

bool MobActionSystem::shearMob(ServerEntity& mob, wire::protocol::play::SoundSource source) {
    world::LoadedWorld* world = worldOf(mob);
    const std::string_view path = mob.type().path();
    if (world == nullptr || mob.removed || mob.dying) {
        return false;
    }
    random::RandomSource random = freshRandom();
    const auto sound = [&](std::string_view name) { playSound(*world, mob, name, 1.0F, 1.0F, source); };
    const auto drop = [&](const ItemStack& stack, double lift, bool scatter) {
        double vx = (random.nextDouble() * 0.2) - 0.1;
        double vz = (random.nextDouble() * 0.2) - 0.1;
        double vy = 0.2;
        if (scatter) {
            vx += static_cast<double>((random.nextFloat() - random.nextFloat()) * 0.1F);
            vy += static_cast<double>(random.nextFloat() * 0.05F);
            vz += static_cast<double>((random.nextFloat() - random.nextFloat()) * 0.1F);
        }
        world->state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                    .uuid = wire::common::Uuid::random(),
                                                    .item = stack.item,
                                                    .count = stack.count,
                                                    .x = mob.x,
                                                    .y = mob.y + lift,
                                                    .z = mob.z,
                                                    .velocityX = vx,
                                                    .velocityY = vy,
                                                    .velocityZ = vz,
                                                    .pickupDelay = world::kSpawnedPickupDelay,
                                                    .components = stack.components});
    };
    // parity: `Entity.dropFromShearingLootTable`, the table run with the sheared mob as the subject.
    const auto table = [&](std::string_view id) {
        std::vector<ItemStack> stacks;
        const server::world::loot::LootTable* found =
            lootTables_ == nullptr ? nullptr : lootTables_->byId(wire::common::Identifier::vanilla(std::string{id}));
        if (found != nullptr) {
            server::world::loot::LootContext context = deathLootContext(*world, mob);
            random::RandomSource& roll =
                found->randomSequence.has_value() ? world->randomSequences().of(*found->randomSequence) : random;
            stacks = found->generate(roll, context.luck, std::nullopt, context);
        }
        return stacks;
    };
    world::vibrations::emit(world->state(), "shear", mob.x, mob.y, mob.z, world->gameTime(), {});
    if (path == "sheep") {
        sound("entity.sheep.shear");
        std::vector<ItemStack> stacks = table("shearing/sheep");
        if (stacks.empty()) {
            const std::int8_t color = mob.sourceNbt.has_value() ? mob.sourceNbt->getByte("Color") : 0;
            stacks.push_back(ItemStack{.item = wire::common::Identifier::vanilla(
                                           std::string{kDyes[static_cast<std::size_t>(color & 15)]} + "_wool"),
                                       .count = 1 + random.nextInt(3)});
        }
        for (const ItemStack& stack : stacks) {
            for (std::int32_t index = 0; index < stack.count; ++index) {
                ItemStack one = stack;
                one.count = 1;
                drop(one, 1.0, true);
            }
        }
        putSource(mob, "Sheared", true);
        return true;
    }
    if (path == "snow_golem") {
        sound("entity.snow_golem.shear");
        putSource(mob, "Pumpkin", false);
        std::vector<ItemStack> stacks = table("shearing/snow_golem");
        if (stacks.empty()) {
            stacks.push_back(ItemStack{.item = wire::common::Identifier::vanilla("carved_pumpkin"), .count = 1});
        }
        for (const ItemStack& stack : stacks) {
            drop(stack, entity::mobBreathing::eyeHeightOf(mob), false);
        }
        return true;
    }
    if (path == "bogged") {
        sound("entity.bogged.shear");
        for (const ItemStack& stack : table("shearing/bogged")) {
            drop(stack, mob.height(), false);
        }
        putSource(mob, "sheared", true);
        return true;
    }
    if (path == "copper_golem") {
        sound("entity.copper_golem.shear");
        const std::optional<ItemStack> worn = equipment::get(mob, equipment::Slot::Saddle);
        equipment::set(mob, equipment::Slot::Saddle, std::nullopt, false);
        equipmentChanged(mob, static_cast<std::uint8_t>(equipment::Slot::Saddle));
        if (worn.has_value()) {
            drop(*worn, 1.5, false);
        }
        return true;
    }
    if (path == "sulfur_cube") {
        const std::optional<ItemStack> body = equipment::get(mob, equipment::Slot::Body);
        equipment::set(mob, equipment::Slot::Body, std::nullopt, false);
        equipmentChanged(mob, static_cast<std::uint8_t>(equipment::Slot::Body));
        if (body.has_value()) {
            drop(*body, mob.height() / 2.0, false);
        }
        return true;
    }
    if (path == "mooshroom") {
        sound("entity.mooshroom.shear");
        const bool brown = mob.sourceNbt.has_value() && mob.sourceNbt->getString("Type") == "brown";
        std::vector<ItemStack> stacks = table("shearing/mooshroom");
        if (stacks.empty()) {
            stacks.push_back(ItemStack{
                .item = wire::common::Identifier::vanilla(brown ? "brown_mushroom" : "red_mushroom"), .count = 5});
        }
        const double x = mob.x;
        const double y = mob.y + 1.0;
        const double z = mob.z;
        const entity::mobConversions::Conversion conversion{.into = wire::common::Identifier::vanilla("cow"),
                                                            .keepEquipment = false,
                                                            .keepCanPickUpLoot = false};
        if (!convertMob(*world, watchersOf(*world), mob, conversion, {})) {
            return false;
        }
        for (const ItemStack& stack : stacks) {
            for (std::int32_t index = 0; index < stack.count; ++index) {
                ItemStack one = stack;
                one.count = 1;
                world->state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                            .uuid = wire::common::Uuid::random(),
                                                            .item = one.item,
                                                            .count = 1,
                                                            .x = x,
                                                            .y = y,
                                                            .z = z,
                                                            .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                            .velocityY = 0.2,
                                                            .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                            .pickupDelay = world::kSpawnedPickupDelay,
                                                            .components = one.components});
            }
        }
        return true;
    }
    return false;
}

} // namespace basalt::server::runtime
