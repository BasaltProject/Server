#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>
#include <span>
#include <string_view>

namespace basalt::server::runtime::world::endCrystals {
namespace {

constexpr std::string_view kBeamTargetKey = "beam_target";
constexpr std::string_view kShowBottomKey = "ShowBottom";

} // namespace

bool isCrystal(const wire::common::Identifier& type) {
    return type.namespaceName() == "minecraft" && type.path() == "end_crystal";
}

std::optional<std::array<std::int32_t, 3>> beamTarget(const entity::ServerEntity& crystal) {
    if (!crystal.sourceNbt.has_value()) {
        return std::nullopt;
    }
    const std::span<const std::int32_t> pos = crystal.sourceNbt->getIntArray(kBeamTargetKey);
    if (pos.size() != 3) {
        return std::nullopt;
    }
    return std::array<std::int32_t, 3>{pos[0], pos[1], pos[2]};
}

void setBeamTarget(entity::ServerEntity& crystal, std::optional<std::array<std::int32_t, 3>> target) {
    if (!crystal.sourceNbt.has_value()) {
        crystal.sourceNbt.emplace();
    }
    if (target.has_value()) {
        crystal.sourceNbt->putIntArray(kBeamTargetKey, {(*target)[0], (*target)[1], (*target)[2]});
    } else {
        static_cast<void>(crystal.sourceNbt->remove(kBeamTargetKey));
    }
}

bool showsBottom(const entity::ServerEntity& crystal) {
    return !crystal.sourceNbt.has_value() || crystal.sourceNbt->getBoolean(kShowBottomKey, true);
}

Hit hurt(entity::ServerEntity& crystal, bool byDragon, bool byExplosion) {
    if (byDragon || crystal.removed) {
        return Hit::Ignored;
    }
    crystal.removed = true;
    crystal.killed = true;
    return byExplosion ? Hit::Removed : Hit::Explodes;
}

std::optional<std::int32_t> keepFireLit(WorldState& world, const entity::ServerEntity& crystal, std::int32_t delay) {
    // parity: `blockPosition`, the block the crystal's feet are in.
    const auto x = static_cast<std::int32_t>(std::floor(crystal.x));
    const auto y = static_cast<std::int32_t>(std::floor(crystal.y));
    const auto z = static_cast<std::int32_t>(std::floor(crystal.z));
    if (!fires::igniteOnImpact(world, x, y, z, delay)) {
        return std::nullopt;
    }
    return world.blockState(x, y, z);
}

} // namespace basalt::server::runtime::world::endCrystals
