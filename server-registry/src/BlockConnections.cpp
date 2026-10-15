#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::server::registry::blockConnections {
namespace {
constexpr std::string_view kBase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr int kAxisX = 0;
constexpr int kAxisZ = 2;

[[nodiscard]] std::vector<std::uint8_t> decode(std::string_view text) {
    while (!text.empty() && text.back() == '=') {
        text.remove_suffix(1);
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() * 6 / 8);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (const char character : text) {
        buffer = (buffer << 6) | static_cast<std::uint32_t>(kBase64.find(character));
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return bytes;
}

[[nodiscard]] const std::unordered_map<std::string_view, std::vector<std::uint8_t>>& families() {
    static const std::unordered_map<std::string_view, std::vector<std::uint8_t>> built = [] {
        std::unordered_map<std::string_view, std::vector<std::uint8_t>> table;
        for (const auto& [name, encoded] : blockFamiliesTable()) {
            table.emplace(name, decode(encoded));
        }
        return table;
    }();
    return built;
}

[[nodiscard]] bool inFamily(std::string_view family, std::int32_t stateId) {
    const auto found = families().find(family);
    if (found == families().end()) {
        return false;
    }
    const std::int32_t blockId = blockSupport::blockRegistryIdOf(stateId);
    if (blockId < 0) {
        return false;
    }
    const auto index = static_cast<std::size_t>(blockId) / 8;
    return index < found->second.size() && ((found->second[index] >> (blockId % 8)) & 1) != 0;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool sameAxis(std::string_view first, std::string_view second) {
    const bool firstZ = first == "north" || first == "south";
    const bool secondZ = second == "north" || second == "south";
    return firstZ == secondZ;
}

[[nodiscard]] std::string_view oppositeOf(std::string_view facing) {
    if (facing == "north") {
        return "south";
    }
    if (facing == "south") {
        return "north";
    }
    if (facing == "west") {
        return "east";
    }
    return "west";
}

[[nodiscard]] std::string_view counterClockwise(std::string_view facing) {
    if (facing == "north") {
        return "west";
    }
    if (facing == "west") {
        return "south";
    }
    if (facing == "south") {
        return "east";
    }
    return "north";
}

// parity: `StairBlock.canTakeShape`: stairs pointing the same way block the corner.
[[nodiscard]] bool canTakeShape(std::string_view facing, const std::string& half, std::int32_t neighbour) {
    if (!isStairs(neighbour)) {
        return true;
    }
    return propertyOf(neighbour, "facing") != facing || propertyOf(neighbour, "half") != half;
}

// parity: `FenceBlock.isSameFence`.
[[nodiscard]] bool sameFence(std::int32_t stateId, std::int32_t neighbourStateId) {
    return inFamily("fences_tag", neighbourStateId) &&
           inFamily("wooden_fences_tag", neighbourStateId) == inFamily("wooden_fences_tag", stateId);
}

// parity: `FenceGateBlock.connectsToDirection`.
[[nodiscard]] bool gateFaces(std::int32_t stateId, BlockFace face) {
    if (!inFamily("gate", stateId)) {
        return false;
    }
    const std::string facing = propertyOf(stateId, "facing");
    if (facing.empty()) {
        return false;
    }
    const int gateAxis = facing == "north" || facing == "south" ? kAxisZ : kAxisX;
    return gateAxis != axisOf(face);
}
} // namespace

bool isFence(std::int32_t stateId) { return inFamily("fence", stateId); }

bool isPanes(std::int32_t stateId) { return inFamily("bars", stateId); }

bool isWall(std::int32_t stateId) { return inFamily("wall", stateId); }

bool isLeaves(std::int32_t stateId) { return inFamily("leaves", stateId); }

bool isStairs(std::int32_t stateId) { return inFamily("stairs", stateId); }

bool connects(std::int32_t stateId) { return isFence(stateId) || isPanes(stateId); }

bool preventsLeafDecay(std::int32_t stateId) { return inFamily("prevents_decay", stateId); }

bool maintainsFarmland(std::int32_t stateId) { return inFamily("maintains_farmland", stateId); }

bool eruptsContinuously(std::int32_t stateId) { return inFamily("geyser_continuous", stateId); }

bool eruptsPeriodically(std::int32_t stateId) { return inFamily("geyser_periodic", stateId); }

bool hasBlockEntity(std::int32_t stateId) { return inFamily("block_entity", stateId); }

bool boostsHatching(std::int32_t stateId) { return inFamily("hatch_boost", stateId); }

bool supportsCactus(std::int32_t stateId) { return inFamily("supports_cactus", stateId); }

bool isFalling(std::int32_t stateId) { return inFamily("falling", stateId); }

bool isSoulFireBase(std::int32_t stateId) { return inFamily("soul_fire_base", stateId); }

std::optional<std::string_view> stairShape(std::int32_t stateId, std::int32_t north, std::int32_t south,
                                           std::int32_t west, std::int32_t east) {
    if (!isStairs(stateId)) {
        return std::nullopt;
    }
    const std::string facing = propertyOf(stateId, "facing");
    const std::string half = propertyOf(stateId, "half");
    if (facing.empty() || half.empty()) {
        return std::nullopt;
    }
    const auto toward = [&](std::string_view direction) {
        if (direction == "north") {
            return north;
        }
        if (direction == "south") {
            return south;
        }
        return direction == "west" ? west : east;
    };

    const std::int32_t behind = toward(facing);
    if (isStairs(behind)) {
        const std::string behindFacing = propertyOf(behind, "facing");
        if (propertyOf(behind, "half") == half && !behindFacing.empty() && !sameAxis(behindFacing, facing) &&
            canTakeShape(facing, half, toward(oppositeOf(behindFacing)))) {
            return behindFacing == counterClockwise(facing) ? "outer_left" : "outer_right";
        }
    }
    const std::int32_t front = toward(oppositeOf(facing));
    if (isStairs(front)) {
        const std::string frontFacing = propertyOf(front, "facing");
        if (propertyOf(front, "half") == half && !frontFacing.empty() && !sameAxis(frontFacing, facing) &&
            canTakeShape(facing, half, toward(frontFacing))) {
            return frontFacing == counterClockwise(facing) ? "inner_left" : "inner_right";
        }
    }
    return "straight";
}

bool connectsTo(std::int32_t stateId, std::int32_t neighbourStateId, BlockFace face) {
    const bool sturdy =
        !inFamily("exception", neighbourStateId) && blockShapes::isFaceSturdy(neighbourStateId, opposite(face));
    if (isFence(stateId)) {
        return sturdy || sameFence(stateId, neighbourStateId) || gateFaces(neighbourStateId, face);
    }
    if (isPanes(stateId)) {
        return sturdy || isPanes(neighbourStateId) || inFamily("walls_tag", neighbourStateId);
    }
    return false;
}
} // namespace basalt::server::registry::blockConnections
