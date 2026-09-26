#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>

namespace basalt::server::runtime::entity::creakings {
namespace {

using wire::nbt::NbtTag;

constexpr std::string_view kHomeKey = "home_pos";
constexpr std::string_view kHeartKey = "creaking";
/// parity: `Creaking.INVULNERABILITY_ANIMATION_DURATION`.
constexpr std::int32_t kInvulnerableTicks = 8;

[[nodiscard]] std::int32_t high(std::uint64_t bits) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32));
}

[[nodiscard]] std::int32_t low(std::uint64_t bits) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(bits));
}

[[nodiscard]] std::uint64_t join(std::int32_t upper, std::int32_t lower) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(upper)) << 32) |
           static_cast<std::uint64_t>(static_cast<std::uint32_t>(lower));
}

} // namespace

bool isCreaking(const wire::common::Identifier& type) { return type.path() == "creaking"; }

CreakingState& stateOf(ServerEntity& creaking) {
    if (creaking.creaking != nullptr) {
        return *creaking.creaking;
    }
    creaking.creaking = std::make_shared<CreakingState>();
    if (creaking.sourceNbt.has_value()) {
        if (const std::span<const std::int32_t> home = creaking.sourceNbt->getIntArray(kHomeKey); home.size() == 3) {
            creaking.creaking->home = std::array<std::int32_t, 3>{home[0], home[1], home[2]};
        }
    }
    return *creaking.creaking;
}

bool heartBound(const ServerEntity& creaking) {
    if (creaking.creaking != nullptr) {
        return creaking.creaking->home.has_value();
    }
    return creaking.sourceNbt.has_value() && creaking.sourceNbt->getIntArray(kHomeKey).size() == 3;
}

void bindTo(ServerEntity& creaking, const std::array<std::int32_t, 3>& home) {
    stateOf(creaking).home = home;
    if (!creaking.sourceNbt.has_value()) {
        creaking.sourceNbt.emplace();
    }
    creaking.sourceNbt->set(std::string{kHomeKey}, NbtTag::ofIntArray({home[0], home[1], home[2]}));
}

bool hurtBound(ServerEntity& creaking, bool fromLivingOrProjectile, bool byPlayer) {
    CreakingState& state = stateOf(creaking);
    if (state.invulnerabilityTicks > 0 || creaking.health <= 0.0 || (!fromLivingOrProjectile && !byPlayer)) {
        return false;
    }
    state.invulnerabilityTicks = kInvulnerableTicks;
    state.hurtPending = true;
    state.hurtByPlayer = byPlayer;
    return true;
}

std::optional<wire::common::Uuid> heartCreaking(const wire::nbt::NbtCompound& heart) {
    const std::span<const std::int32_t> ints = heart.getIntArray(kHeartKey);
    if (ints.size() != 4) {
        return std::nullopt;
    }
    return wire::common::Uuid{join(ints[0], ints[1]), join(ints[2], ints[3])};
}

void setHeartCreaking(wire::nbt::NbtCompound& heart, const std::optional<wire::common::Uuid>& creaking) {
    if (!creaking.has_value()) {
        static_cast<void>(heart.remove(kHeartKey));
        return;
    }
    heart.set(std::string{kHeartKey}, NbtTag::ofIntArray({high(creaking->high()), low(creaking->high()),
                                                          high(creaking->low()), low(creaking->low())}));
}

std::int32_t signalFor(double distance) {
    const double scaled = std::clamp(distance, 0.0, 32.0) / 32.0;
    return 15 - static_cast<std::int32_t>(std::floor(scaled * 15.0));
}

} // namespace basalt::server::runtime::entity::creakings
