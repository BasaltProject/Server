#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Beds.hpp>

#include <map>

namespace basalt::server::runtime::world {
namespace {

constexpr std::int64_t kDayTicks = 24000;

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

} // namespace

std::string_view translationKeyOf(BedProblem problem) {
    switch (problem) {
    case BedProblem::NotPossibleNow:
        return "block.minecraft.bed.no_sleep";
    case BedProblem::TooFarAway:
        return "block.minecraft.bed.too_far_away";
    case BedProblem::Occupied:
        return "block.minecraft.bed.occupied";
    case BedProblem::NotHere:
        break;
    }
    return "block.minecraft.bed.not_valid";
}

namespace beds {

bool isBed(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    return id != nullptr && std::string_view{id->path()}.ends_with("_bed");
}

bool isOccupied(std::int32_t stateId) { return propertyOf(stateId, "occupied") == "true"; }

std::string facingOf(std::int32_t stateId) { return propertyOf(stateId, "facing"); }

bool isHead(std::int32_t stateId) { return propertyOf(stateId, "part") == "head"; }

std::int32_t withOccupied(std::int32_t stateId, bool occupied) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    return registry::blockStateDefinitions::stateIdOf(*block, {{"occupied", occupied ? "true" : "false"}}, stateId)
        .value_or(stateId);
}

std::array<std::int32_t, 3> headOffset(std::int32_t stateId) {
    const std::string facing = facingOf(stateId);
    if (facing == "north") {
        return {0, 0, -1};
    }
    if (facing == "south") {
        return {0, 0, 1};
    }
    if (facing == "west") {
        return {-1, 0, 0};
    }
    if (facing == "east") {
        return {1, 0, 0};
    }
    return {0, 0, 0};
}

bool explodesIn(config::WorldDimension dimension) { return dimension != config::WorldDimension::Overworld; }

bool isNight(std::int64_t timeOfDay) {
    const std::int64_t inDay = timeOfDay % kDayTicks;
    return inDay >= kNightStart && inDay <= kNightEnd;
}

std::int64_t nextMorning(std::int64_t timeOfDay) { return ((timeOfDay / kDayTicks) + 1) * kDayTicks; }

std::vector<BlockChange> destroy(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    if (!state.has_value() || !isBed(*state)) {
        return {};
    }
    const std::array<std::int32_t, 3> offset = headOffset(*state);
    const std::array<std::int32_t, 3> other = isHead(*state)
                                                  ? std::array<std::int32_t, 3>{x - offset[0], y, z - offset[2]}
                                                  : std::array<std::int32_t, 3>{x + offset[0], y, z + offset[2]};
    std::vector<BlockChange> removed;
    for (const std::array<std::int32_t, 3>& at : {std::array<std::int32_t, 3>{x, y, z}, other}) {
        const std::optional<std::int32_t> here = world.blockState(at[0], at[1], at[2]);
        if (!here.has_value() || !isBed(*here)) {
            continue;
        }
        const std::int32_t air = registry::blocks::air().stateId;
        static_cast<void>(world.setBlockState(at[0], at[1], at[2], air));
        removed.push_back(BlockChange{.x = at[0], .y = at[1], .z = at[2], .stateId = air});
    }
    return removed;
}

bool isBedItem(const wire::common::Identifier& item) { return std::string_view{item.path()}.ends_with("_bed"); }

} // namespace beds

} // namespace basalt::server::runtime::world
