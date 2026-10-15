#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ChorusFlowerGrowth.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::chorusFlowerGrowth {
namespace {

/// parity: `#minecraft:supports_chorus_flower`, which is end stone and little else.
constexpr std::array<std::string_view, 1> kGround{"end_stone"};

[[nodiscard]] bool isGround(std::string_view path) {
    return std::find(kGround.begin(), kGround.end(), path) != kGround.end();
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string pathAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() ? pathOf(*stateId) : std::string{};
}

void flower(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t age,
            std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("chorus_flower"), {{"age", std::to_string(age)}}, std::nullopt);
    if (!state.has_value()) {
        return;
    }
    growthSupport::set(world, x, y, z, *state, changes);
}

void plant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const registry::BlockState* stem = registry::blocks::named("chorus_plant");
    if (stem == nullptr) {
        return;
    }
    growthSupport::set(world, x, y, z, stem->stateId, changes);
}

/// parity: `ChorusFlowerBlock.allNeighborsEmpty`, which lets one side through.
[[nodiscard]] bool sidesEmpty(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                              std::string_view ignore) {
    for (const std::string_view facing : growthSupport::kHorizontalNames) {
        if (facing == ignore) {
            continue;
        }
        const std::int32_t sideX = x + growthSupport::offsetX(facing);
        const std::int32_t sideZ = z + growthSupport::offsetZ(facing);
        if (world.blockState(sideX, y, sideZ) != registry::blocks::air().stateId) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string_view opposite(std::string_view facing) {
    if (facing == "north") {
        return "south";
    }
    if (facing == "south") {
        return "north";
    }
    return facing == "west" ? "east" : "west";
}

} // namespace

void chorusFlower(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  random::RandomSource& random, std::vector<BlockChange>& changes) {
    const std::int32_t air = registry::blocks::air().stateId;
    // parity: `above.getY() <= level.getMaxY()` needs no test of its own here.
    if (world.blockState(x, y + 1, z) != air) {
        return;
    }
    const std::optional<std::int32_t> age = growthSupport::ageOf(stateId);
    if (!age.has_value() || *age >= kDeadAge) {
        return;
    }

    bool grows = false;
    bool onPillar = false;
    const std::optional<std::int32_t> below = world.blockState(x, y - 1, z);
    if (!below.has_value()) {
        return;
    }
    const std::string belowPath = pathOf(*below);
    if (isGround(belowPath)) {
        grows = true;
    } else if (belowPath == "chorus_plant") {
        // parity: a stem taller than four is measured no further, and the roll that lets it keep.
        std::int32_t height = 1;
        for (std::int32_t at = 0; at < 4; ++at) {
            const std::string test = pathAt(world, x, y - height - 1, z);
            if (test != "chorus_plant") {
                if (isGround(test)) {
                    onPillar = true;
                }
                break;
            }
            ++height;
        }
        if (height < 2 || height <= random.nextInt(onPillar ? 5 : 4)) {
            grows = true;
        }
    } else if (*below == air) {
        grows = true;
    }

    if (grows && sidesEmpty(world, x, y + 1, z, {}) && world.blockState(x, y + 2, z) == air) {
        plant(world, x, y, z, changes);
        flower(world, x, y + 1, z, *age, changes);
        return;
    }
    if (*age >= kBranchAge) {
        flower(world, x, y, z, kDeadAge, changes);
        return;
    }

    std::int32_t attempts = random.nextInt(4);
    if (onPillar) {
        ++attempts;
    }
    bool branched = false;
    for (std::int32_t at = 0; at < attempts; ++at) {
        const std::string_view facing = growthSupport::kHorizontalNames[static_cast<std::size_t>(
            random.nextInt(static_cast<std::int32_t>(growthSupport::kHorizontalNames.size())))];
        const std::int32_t branchX = x + growthSupport::offsetX(facing);
        const std::int32_t branchZ = z + growthSupport::offsetZ(facing);
        if (world.blockState(branchX, y, branchZ) != air) {
            continue;
        }
        if (world.blockState(branchX, y - 1, branchZ) != air) {
            continue;
        }
        if (!sidesEmpty(world, branchX, y, branchZ, opposite(facing))) {
            continue;
        }
        flower(world, branchX, y, branchZ, *age + 1, changes);
        branched = true;
    }
    if (branched) {
        plant(world, x, y, z, changes);
    } else {
        flower(world, x, y, z, kDeadAge, changes);
    }
}

} // namespace basalt::server::runtime::world::chorusFlowerGrowth
