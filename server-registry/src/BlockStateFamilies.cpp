#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::registry::blockStateFamilies {
namespace {

[[nodiscard]] std::int32_t defaultState(std::string_view path) {
    const BlockState* found = blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::vector<std::int32_t> defaultStates(std::initializer_list<std::string_view> paths) {
    std::vector<std::int32_t> ids;
    ids.reserve(paths.size());
    for (const std::string_view path : paths) {
        ids.push_back(defaultState(path));
    }
    return ids;
}

/// Whether the id falls in `[first, first + span]` of any of those family defaults, and which.
[[nodiscard]] std::optional<std::int32_t> familyStart(const std::vector<std::int32_t>& defaults, std::int32_t stateId,
                                                      std::int32_t span) {
    for (const std::int32_t candidate : defaults) {
        if (stateId >= candidate && stateId <= candidate + span) {
            return candidate;
        }
    }
    return std::nullopt;
}

const std::vector<std::int32_t>& doublePlantDefaults() {
    static const std::vector<std::int32_t> ids =
        defaultStates({"sunflower", "lilac", "rose_bush", "peony", "tall_grass", "large_fern", "tall_seagrass"});
    return ids;
}

const std::vector<std::int32_t>& snowyDefaults() {
    static const std::vector<std::int32_t> ids = defaultStates({"grass_block", "podzol", "mycelium"});
    return ids;
}

const std::vector<std::int32_t>& coralWaterloggedDefaults() {
    static const std::vector<std::int32_t> ids =
        defaultStates({"tube_coral", "brain_coral", "bubble_coral", "fire_coral", "horn_coral", "tube_coral_fan",
                       "brain_coral_fan", "bubble_coral_fan", "fire_coral_fan", "horn_coral_fan"});
    return ids;
}

const std::vector<std::int32_t>& coralWallFanDefaults() {
    static const std::vector<std::int32_t> ids =
        defaultStates({"tube_coral_wall_fan", "brain_coral_wall_fan", "bubble_coral_wall_fan", "fire_coral_wall_fan",
                       "horn_coral_wall_fan"});
    return ids;
}

const std::vector<std::int32_t>& hugeMushroomDefaults() {
    static const std::vector<std::int32_t> ids =
        defaultStates({"brown_mushroom_block", "red_mushroom_block", "mushroom_stem"});
    return ids;
}

const std::vector<std::int32_t>& growingVineHeadDefaults() {
    static const std::vector<std::int32_t> ids = defaultStates({"weeping_vines", "twisting_vines"});
    return ids;
}

[[nodiscard]] std::int32_t leavesFirst() {
    static const std::int32_t first = defaultState("oak_leaves") - 27;
    return first;
}

[[nodiscard]] std::int32_t leavesLast() {
    static const std::int32_t last = defaultState("flowering_azalea_leaves");
    return last;
}

[[nodiscard]] std::int32_t leafStateRangeStart(std::int32_t stateId) {
    return leavesFirst() + ((stateId - leavesFirst()) / 28) * 28;
}

[[nodiscard]] std::optional<std::int32_t> horizontalFacingOffset(std::string_view facing) {
    static const std::map<std::string, std::int32_t, std::less<>> offsets = {
        {"north", 0}, {"south", 1}, {"west", 2}, {"east", 3}};
    const auto found = offsets.find(facing);
    if (found == offsets.end()) {
        return std::nullopt;
    }
    return found->second;
}

} // namespace

bool isLeavesState(std::int32_t stateId) { return stateId >= leavesFirst() && stateId <= leavesLast(); }

std::optional<std::int32_t> leafDistance(std::int32_t stateId) {
    if (!isLeavesState(stateId)) {
        return std::nullopt;
    }
    return ((stateId - leafStateRangeStart(stateId)) / 4) + 1;
}

std::optional<std::int32_t> withLeafDistance(std::int32_t stateId, std::int32_t distance) {
    if (distance < 1 || distance > 7) {
        throw std::invalid_argument{"leaf distance must be 1 through 7"};
    }
    if (!isLeavesState(stateId)) {
        return std::nullopt;
    }
    // The four persistent/waterlogged states sit in order under each distance, counted from the
    // block's own first state: the range is not aligned to four since 26.3.
    const std::int32_t start = leafStateRangeStart(stateId);
    return start + (distance - 1) * 4 + (stateId - start) % 4;
}

bool isDoublePlantState(std::int32_t stateId) {
    return std::ranges::any_of(doublePlantDefaults(), [stateId](std::int32_t candidate) {
        return stateId >= candidate - 1 && stateId <= candidate;
    });
}

std::optional<std::int32_t> upperDoublePlantStateId(std::int32_t lowerStateId) {
    if (std::ranges::find(doublePlantDefaults(), lowerStateId) == doublePlantDefaults().end()) {
        return std::nullopt;
    }
    return lowerStateId - 1;
}

std::optional<std::int32_t> withSnowy(std::int32_t stateId, bool snowy) {
    for (const std::int32_t candidate : snowyDefaults()) {
        if (stateId >= candidate - 1 && stateId <= candidate) {
            return snowy ? candidate - 1 : candidate;
        }
    }
    return std::nullopt;
}

bool isKelpState(std::int32_t stateId) {
    const std::int32_t kelp = defaultState("kelp");
    return stateId >= kelp && stateId <= kelp + 25;
}

std::optional<std::int32_t> growingVineHeadStateId(std::int32_t stateId, std::int32_t age) {
    if (age < 0 || age > 25) {
        throw std::invalid_argument{"growing vine age must be 0 through 25"};
    }
    const std::optional<std::int32_t> start = familyStart(growingVineHeadDefaults(), stateId, 25);
    if (!start.has_value()) {
        return std::nullopt;
    }
    return *start + age;
}

bool isSeaPickleState(std::int32_t stateId) {
    const std::int32_t seaPickle = defaultState("sea_pickle");
    return stateId >= seaPickle && stateId <= seaPickle + 7;
}

bool isCoralPlantState(std::int32_t stateId) {
    return familyStart(coralWaterloggedDefaults(), stateId, 1).has_value() ||
           familyStart(coralWallFanDefaults(), stateId, 7).has_value();
}

std::optional<std::int32_t> coralWallFanStateId(std::int32_t stateId, std::string_view facing, bool waterlogged) {
    const std::optional<std::int32_t> start = familyStart(coralWallFanDefaults(), stateId, 7);
    if (!start.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> facingOffset = horizontalFacingOffset(facing);
    if (!facingOffset.has_value()) {
        return std::nullopt;
    }
    return *start + *facingOffset * 2 + (waterlogged ? 0 : 1);
}

bool isHugeMushroomState(std::int32_t stateId) { return familyStart(hugeMushroomDefaults(), stateId, 63).has_value(); }

std::optional<std::int32_t> hugeMushroomStateId(std::int32_t stateId, bool down, bool east, bool north, bool south,
                                                bool up, bool west) {
    const std::optional<std::int32_t> start = familyStart(hugeMushroomDefaults(), stateId, 63);
    if (!start.has_value()) {
        return std::nullopt;
    }
    std::int32_t offset = 0;
    if (!down) {
        offset += 32;
    }
    if (!east) {
        offset += 16;
    }
    if (!north) {
        offset += 8;
    }
    if (!south) {
        offset += 4;
    }
    if (!up) {
        offset += 2;
    }
    if (!west) {
        ++offset;
    }
    return *start + offset;
}

bool isWildflowersState(std::int32_t stateId) {
    const std::int32_t first = defaultState("wildflowers");
    return stateId >= first && stateId <= first + 15;
}

bool isPinkPetalsState(std::int32_t stateId) {
    const std::int32_t first = defaultState("pink_petals");
    return stateId >= first && stateId <= first + 15;
}

bool isGlowLichenState(std::int32_t stateId) {
    const std::int32_t last = defaultState("glow_lichen");
    return stateId >= last - 127 && stateId <= last;
}

bool isVineState(std::int32_t stateId) {
    const std::int32_t last = defaultState("vine");
    return stateId >= last - 31 && stateId <= last;
}

bool isCocoaState(std::int32_t stateId) {
    const std::int32_t first = defaultState("cocoa");
    return stateId >= first && stateId <= first + 11;
}

bool isPaleHangingMossState(std::int32_t stateId) {
    const std::int32_t first = defaultState("pale_hanging_moss");
    return stateId >= first && stateId <= first + 1;
}

bool isCreakingHeartState(std::int32_t stateId) {
    const std::int32_t defaultStateId = defaultState("creaking_heart");
    return stateId >= defaultStateId - 7 && stateId <= defaultStateId + 10;
}

bool isMangrovePropaguleState(std::int32_t stateId) {
    const std::int32_t defaultStateId = defaultState("mangrove_propagule");
    return stateId >= defaultStateId - 5 && stateId <= defaultStateId + 34;
}

bool isLeafLitterState(std::int32_t stateId) {
    const std::int32_t first = defaultState("leaf_litter");
    return stateId >= first && stateId <= first + 15;
}

bool isBeeNestState(std::int32_t stateId) {
    const std::int32_t first = defaultState("bee_nest");
    return stateId >= first && stateId <= first + 23;
}

std::int32_t chorusPlantStateId(bool down, bool east, bool north, bool south, bool up, bool west) {
    std::int32_t enabledBits = 0;
    if (down) {
        enabledBits += 32;
    }
    if (east) {
        enabledBits += 16;
    }
    if (north) {
        enabledBits += 8;
    }
    if (south) {
        enabledBits += 4;
    }
    if (up) {
        enabledBits += 2;
    }
    if (west) {
        ++enabledBits;
    }
    return defaultState("chorus_plant") - enabledBits;
}

std::int32_t chorusFlowerStateId(std::int32_t age) {
    if (age < 0 || age > 5) {
        throw std::invalid_argument{"chorus flower age must be 0 through 5"};
    }
    return defaultState("chorus_flower") + age;
}

std::int32_t ironBarsStateId(bool north, bool east, bool south, bool west) {
    std::int32_t enabledBits = 0;
    if (east) {
        enabledBits += 16;
    }
    if (north) {
        enabledBits += 8;
    }
    if (south) {
        enabledBits += 4;
    }
    if (west) {
        ++enabledBits;
    }
    return defaultState("iron_bars") - enabledBits;
}

} // namespace basalt::server::registry::blockStateFamilies
