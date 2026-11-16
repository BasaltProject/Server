#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerBerries.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <map>
#include <string>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;
using wire::common::Identifier;

constexpr double kPopJitter = 0.25;
constexpr double kPopSpread = 0.2;
constexpr double kItemHalfHeight = 0.125;
constexpr std::int32_t kPickupDelay = 10;

[[nodiscard]] double roll(const BreakContext& context) { return context.nextDouble ? context.nextDouble() : 0.5; }

/// parity: `Block.popResource`.
void pop(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
         const Identifier& item, std::int32_t count) {
    if (count <= 0 || !world.gameRules().get(world::gameRules::kBlockDrops)) {
        return;
    }
    const auto jitter = [&context] { return (roll(context) * 2.0 * kPopJitter) - kPopJitter; };
    spawnDroppedItem(context, world, item, count, x + 0.5 + jitter(), y + 0.5 + jitter() - kItemHalfHeight,
                     z + 0.5 + jitter(), (roll(context) * 2.0 * kPopSpread) - kPopSpread / 2.0, kPopSpread,
                     (roll(context) * 2.0 * kPopSpread) - kPopSpread / 2.0, kPickupDelay, {});
}

} // namespace

bool harvestBerries(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                    std::int32_t y, std::int32_t z, const HarvestGate& gate) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const Identifier* block = reg::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*stateId);
    std::map<std::string, std::string> next = properties;
    std::string_view item;
    std::string_view sound;
    std::int32_t count = 0;
    float pitch = 1.0F;
    if (path == "sweet_berry_bush") {
        const auto age = properties.find("age");
        if (age == properties.end() || std::stoi(age->second) < 2) {
            return false;
        }
        const bool ripe = age->second == "3";
        item = "sweet_berries";
        sound = "block.sweet_berry_bush.pick_berries";
        count = (ripe ? 2 : 1) + static_cast<std::int32_t>(roll(context) * 2.0);
        pitch = 0.8F + static_cast<float>(roll(context)) * 0.4F;
        next["age"] = "1";
    } else if (path == "cave_vines" || path == "cave_vines_plant") {
        const auto berries = properties.find("berries");
        if (berries == properties.end() || berries->second != "true") {
            return false;
        }
        item = "glow_berries";
        sound = "block.cave_vines.pick_berries";
        count = 1;
        pitch = 0.8F + static_cast<float>(roll(context)) * 0.4F;
        next["berries"] = "false";
    } else {
        return false;
    }
    if (gate && !gate(player, x, y, z, path, item, count)) {
        return true;
    }
    const std::optional<std::int32_t> after = reg::blockStateDefinitions::stateIdOf(*block, next, *stateId);
    if (!after.has_value() || !world.state().setBlockState(x, y, z, *after)) {
        return true;
    }
    pop(context, world, x, y, z, Identifier::vanilla(std::string{item}), count);
    if (context.blocks != nullptr) {
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, *after,
                                    {world::blockEffect::Sound{.name = std::string{sound}, .pitch = pitch}});
    }
    announceBlock(context, world.name(), x, y, z, *after);
    notifyAround(context, world, x, y, z, std::nullopt);
    return true;
}

} // namespace basalt::server::runtime::player
