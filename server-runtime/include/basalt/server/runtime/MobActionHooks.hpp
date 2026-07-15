#pragma once

#include <basalt/server/runtime/VillagerWorldSystem.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Leashes.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class AdvancementTriggers;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::world {
class RealRecipes;
}

namespace basalt::server::runtime::entity::allays {
struct AllayState;
}

namespace basalt::server::runtime::entity::piglins {
struct PiglinState;
}

namespace basalt::server::runtime::entity::neutralAnger {
struct Anger;
}

namespace basalt::server::runtime::entity::frogs {
struct FrogState;
}

namespace basalt::server::runtime::entity::sniffers {
struct SnifferState;
}

namespace basalt::server::runtime::entity::armadillos {
struct ArmadilloState;
}

namespace basalt::server::runtime::entity::breezes {
struct BreezeState;
}

namespace basalt::server::runtime::entity::goats {
struct GoatState;
}

namespace basalt::server::runtime::scoreboard {
class Teams;
}

namespace basalt::server::runtime {

struct MobActionHooks {
    entity::entityCombat::Hooks combat;
    std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::int32_t fireTicks,
                       std::string_view cause)>
        combustAllowed;
    std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& mob,
                       const std::optional<std::int32_t>& previous, std::optional<std::int32_t>& next)>
        targetChanged;
    std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& mob,
                       const world::ActiveEffect& effect, std::string_view action)>
        effectAllowed;
    std::function<void(const world::LoadedWorld& world, const entity::ServerEntity& mob,
                       std::vector<server::world::loot::ItemStack>& drops, std::int32_t& experience)>
        deathDrops;
    std::function<bool(const player::PlayerSession& player, const entity::ServerEntity& animal)> tameAllowed;
    std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& animal,
                       const entity::ServerEntity& partner)>
        breedAllowed;
    std::function<void(std::string_view cause, const wire::common::Identifier& parent,
                       const wire::common::Identifier& partner, const wire::common::Identifier& child)>
        bredAnimals;
    std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t value)> spawnOrb;
    std::function<void(world::LoadedWorld& world, entity::ServerEntity& wither)> witherShoots;
    std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t openers)>
        openersChanged;
    std::function<void(const std::string& from, const std::string& to)> teamCarried;
    std::function<void(const wire::common::Uuid& player)> curedZombieVillager;
    std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view key,
                       entity::ai::PluginGoalPhase phase)>
        pluginGoal;
    std::function<void(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y, double z,
                       std::int32_t ownerId)>
        launchFirework;
};

struct MobActionOptions {
    std::function<std::int64_t()> nextSeed;
    const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
    const scoreboard::Teams* teams = nullptr;
    std::function<double()> nextDouble;
    const world::RealRecipes* recipes = nullptr;
    const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
    const world::realBlockTags::TagMembers* blockTags = nullptr;
    const world::realItemTags::TagMembers* itemTags = nullptr;
    world::RealVillagerTrades* villagerTrades = nullptr;
    world::AdvancementTriggers* advancementTriggers = nullptr;
};

} // namespace basalt::server::runtime
