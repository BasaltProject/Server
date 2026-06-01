#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/JavaCast.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot {

struct LootFunction::Parts {
    IntProvider count;
    FloatProvider fraction;
    std::optional<IntProvider> least;
    std::optional<IntProvider> most;
    std::string enchantment;
    std::vector<std::string> options;
    std::vector<std::pair<std::string, IntProvider>> levels;
    wire::nbt::NbtCompound components;
    std::vector<std::string> removed;
    std::vector<LootFunction> functions;
    ItemPredicate filter;
    std::vector<LootFunction> onPass;
    std::vector<LootFunction> onFail;
    ExplorationMapRequest exploration;
    std::optional<EntityTarget> resolveFor;
    std::vector<std::string> resolveKeys;
};

[[nodiscard]] std::map<std::string, std::int32_t, std::less<>> enchantmentsOf(const ItemStack& stack);

namespace lootMath {

/// parity: `Math.round(float)`, half up, saturating, and 0 for NaN.
[[nodiscard]] inline std::int32_t round(float value) {
    return wire::common::toIntLikeJava(std::floor(static_cast<double>(value) + 0.5));
}

/// parity: `Mth.nextInt`, inclusive and short-circuiting when the ends meet.
[[nodiscard]] inline std::int32_t between(random::RandomSource& random, std::int32_t least, std::int32_t most) {
    return least >= most ? least : random.nextInt(most - least + 1) + least;
}

} // namespace lootMath

} // namespace basalt::server::world::loot
