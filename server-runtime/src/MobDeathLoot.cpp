#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>

#include <array>
#include <expected>
#include <string_view>

namespace basalt::server::runtime {
namespace {

/// The damage type behind the name `entityCombat::hurt` was given, where the two differ.
[[nodiscard]] wire::common::Identifier damageTypeOf(std::string_view cause) {
    if (cause == "attack" || cause == "sweep") {
        return wire::common::Identifier::vanilla("player_attack");
    }
    if (cause == "projectile") {
        return wire::common::Identifier::vanilla("arrow");
    }
    return wire::common::Identifier::vanilla(std::string{cause});
}

using ::basalt::server::world::loot::EntityFacts;
using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;

/// parity: `DyeColor`, in the order a sheep's `Color` byte counts them.
constexpr std::array<std::string_view, 16> kDyeColors{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                      "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                      "brown", "green",  "red",        "black"};

[[nodiscard]] EntityFacts factsOf(const world::LoadedWorld& world, const ServerEntity& entity) {
    EntityFacts facts{.type = entity.type(),
                      .onFire = entity.fireTicks > 0,
                      .baby = entity.age < 0,
                      .cubeSize = entity.cubeSize,
                      .captain = entity.patrolLeader,
                      .scoreName = entity.uuid().toString()};
    const std::string_view path = entity.type().path();
    if (entity.sourceNbt.has_value()) {
        const wire::nbt::NbtCompound& data = *entity.sourceNbt;
        if (path == "sheep") {
            facts.components.emplace("minecraft:sheep/color",
                                     std::string{kDyeColors[static_cast<std::size_t>(data.getByte("Color") & 0x0F)]});
            facts.sheared = data.getBoolean("Sheared");
        } else if (path == "mooshroom") {
            facts.components.emplace("minecraft:mooshroom/variant",
                                     data.getString("Type").empty() ? "red" : std::string{data.getString("Type")});
        }
        facts.baby = facts.baby || data.getBoolean("IsBaby");
    } else if (path == "sheep") {
        facts.components.emplace("minecraft:sheep/color", "white");
    }
    if (entity.vehicleId.has_value()) {
        if (const ServerEntity* vehicle = world.state().entity(*entity.vehicleId); vehicle != nullptr) {
            facts.vehicle = vehicle->type();
        }
    }
    return facts;
}

void addHeld(EntityFacts& facts, const std::optional<storage::StoredItemStack>& held) {
    if (!held.has_value()) {
        return;
    }
    facts.mainhand = Identifier::parse(held->id).value_or(Identifier::vanilla("air"));
    for (const auto& [enchantment, level] : player::enchantmentLevels(*held)) {
        facts.mainhandEnchantments.emplace(enchantment.toString(), level);
    }
}

} // namespace

::basalt::server::world::loot::LootContext MobActionSystem::deathLootContext(world::LoadedWorld& world,
                                                                             const ServerEntity& dead) const {
    ::basalt::server::world::loot::LootContext context{.hasEntity = true};
    context.thisEntity = factsOf(world, dead);
    // parity: `LAST_DAMAGE_PLAYER`, the player whose hit is still remembered.
    context.killedByPlayer = dead.lastHurtByPlayerTime > 0;
    // parity: `SmeltItemFunction`, the first smelting recipe that takes the item.
    if (const world::RealRecipes* recipes = options_.recipes; recipes != nullptr) {
        context.smelt = [recipes](const ::basalt::server::world::loot::ItemStack& input)
            -> std::optional<::basalt::server::world::loot::ItemStack> {
            const ::basalt::server::world::crafting::Recipe* recipe =
                recipes->cookingFor(::basalt::server::world::crafting::Cooker::Smelting, input);
            if (recipe == nullptr) {
                return std::nullopt;
            }
            return recipe->result;
        };
    }
    world::RealLootTables* tables = lootTables_;
    tables->prepareAt(context, world, dead.x, dead.y, dead.z);
    // parity: `damage_source_properties` reads the source of the killing blow, by its damage type.
    if (!dead.lastDamageCause.empty() && options_.damageTypeTags != nullptr) {
        const Identifier type = damageTypeOf(dead.lastDamageCause);
        context.damageType = type;
        context.damageTagged = [tags = options_.damageTypeTags, type](std::string_view tag) {
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(tag);
            return id.has_value() && world::realDamageTypeTags::isIn(*tags, *id, type);
        };
    }
    const world::realEntityTypeTags::TagMembers* entityTags = options_.entityTags;
    context.tags = [entityTags](std::string_view registry, const Identifier& id, std::string_view tag) {
        if (registry == "entity_type") {
            return entityTags != nullptr &&
                   world::realEntityTypeTags::membersOf(*entityTags, Identifier::parse(tag).value_or(id)).contains(id);
        }
        // parity: `#minecraft:smelts_loot`, which is fire aspect and nothing else.
        return registry == "enchantment" && tag == "minecraft:smelts_loot" && id == Identifier::vanilla("fire_aspect");
    };
    // parity: `LAST_DAMAGE_PLAYER`, the player whose hit is still remembered, and their luck.
    const auto playerFacts = [](const PlayerSession& player) {
        EntityFacts facts{.type = Identifier::vanilla("player"), .scoreName = player.name()};
        addHeld(facts, player.inventory.heldItem());
        return facts;
    };
    const auto playerById = [this](std::int32_t entityId) -> const PlayerSession* {
        for (const PlayerSession* player : players_->snapshot()) {
            if (player->entityId() == entityId) {
                return player;
            }
        }
        return nullptr;
    };
    if (context.killedByPlayer && dead.lastHurtByPlayer.has_value()) {
        if (const PlayerSession* killer = players_->get(*dead.lastHurtByPlayer); killer != nullptr) {
            context.attackingPlayer = playerFacts(*killer);
            context.luck = killer->luck();
        }
    }
    context.killedByPlayer = context.attackingPlayer.has_value();
    // parity: `ATTACKING_ENTITY` and `DIRECT_ATTACKING_ENTITY` come off the killing blow's source:
    // who is behind it, and what landed it, the arrow or the fireball rather than its shooter.
    const ServerEntity::Blame& blame = dead.lastBlame;
    const auto factsById = [&](std::int32_t entityId) -> std::optional<EntityFacts> {
        if (const PlayerSession* player = playerById(entityId); player != nullptr) {
            return playerFacts(*player);
        }
        if (const ServerEntity* mob = world.state().entity(entityId); mob != nullptr) {
            return factsOf(world, *mob);
        }
        return std::nullopt;
    };
    if (blame.causingId.has_value()) {
        context.attacker = factsById(*blame.causingId);
        if (context.attacker.has_value() && context.attacker->type == Identifier::vanilla("player")) {
            context.attackerEnchantments = context.attacker->mainhandEnchantments;
        }
    }
    if (blame.directId.has_value()) {
        context.directAttacker = blame.directId == blame.causingId ? context.attacker : factsById(*blame.directId);
        // A fireball that went off is gone by the time the body drops; its kind is what is left.
        if (!context.directAttacker.has_value() && blame.directType.has_value()) {
            context.directAttacker = EntityFacts{.type = *blame.directType};
        }
    }
    return context;
}

} // namespace basalt::server::runtime
