#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GolemPatterns.hpp>
#include <basalt/server/runtime/world/WitherPatterns.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `DispenserBlock.neighborChanged`.
namespace dispensers {

inline constexpr std::int32_t kSize = 9;

[[nodiscard]] const wire::common::Identifier& tickType();

/// parity: `DispenserBlock.neighborChanged`.
[[nodiscard]] std::optional<BlockChange> onNeighbourChange(WorldState& world, std::string_view worldName,
                                                           std::int32_t x, std::int32_t y, std::int32_t z);

/// Every field falls back to the registry when unset, so `{}` gives vanilla behaviour.
struct Hooks {
    std::function<std::int32_t()> allocateEntityId;
    /// False cancels the dispense of the picked slot before anything happens, as `BlockPreDispenseEvent` does.
    std::function<bool(const server::world::loot::ItemStack& stack, std::int32_t slot)> preDispense;
    /// False silences the click of an empty dispenser, as `BlockFailedDispenseEvent` does.
    std::function<bool()> failedEffect;
    /// False cancels the shot of the chosen stack, as `BlockDispenseEvent` does.
    std::function<bool(const server::world::loot::ItemStack& stack, server::registry::BlockFace out)> dispensing;
    /// Unset, only snow and iron golems can stand up from a carved pumpkin.
    golemPatterns::IsCopper isCopper;
    std::function<std::optional<std::int32_t>(std::string_view path)> containerSize;
    std::function<std::optional<std::int32_t>(const wire::common::Identifier& type)> protocolTypeOf;
    /// parity: `HopperBlockEntity.addItem`.
    std::function<const server::world::crafting::Recipe*(server::world::crafting::Cooker,
                                                         const server::world::loot::ItemStack&)>
        recipeFor;
    /// parity: `FLUID_PLACE`.
    std::int64_t gameTime = 0;
    /// Unset, a dispenser never finishes a wither.
    witherPatterns::IsBase isWitherBase;
    /// parity: `WitherSkullBlock.checkSpawn` from the dispenser behaviour.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> skullPlaced;
    /// parity: `BlockTags.CONVERTABLE_TO_MUD`.
    std::function<bool(const wire::common::Identifier& block)> convertsToMud;
    /// parity: `FireworkRocketItem.createDispenseConfig`.
    std::function<void(double x, double y, double z, double dirX, double dirY, double dirZ,
                       const server::world::loot::ItemStack& stack)>
        launchFirework;
    /// parity: `BeehiveBlock.releaseBeesAndResetHoneyLevel(..., BEE_RELEASED)`.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)> releaseHive;
    /// Shears whatever is ready in the square in front; false when nothing was.
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, const server::world::loot::ItemStack& tool)>
        shearEntity;
    /// Dresses whoever stands in the square in front with one item; false when nobody takes it.
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, const server::world::loot::ItemStack& stack)>
        equipEntity;
    /// parity: `Armadillo.brushOffScute`, the loot, sound and interaction event of a brushing.
    std::function<bool(entity::ServerEntity& armadillo)> brushArmadillo;
    /// parity: `SpawnEggItemBehavior`, the mob finalized with `EntitySpawnReason.DISPENSER`. Unset, it spawns bare.
    std::function<void(const wire::common::Identifier& type, double x, double y, double z)> spawnEgg;
};

struct Result {
    bool fired = false;
    std::vector<BlockEffect> effects;
    std::vector<BlockChange> changes;
};

/// parity: `DispenserBlock.dispenseFrom`.
[[nodiscard]] Result fire(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                          const Hooks& hooks = {});

} // namespace dispensers

} // namespace basalt::server::runtime::world
