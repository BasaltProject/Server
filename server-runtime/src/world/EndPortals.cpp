#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/EndPortals.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::endPortals {
namespace {

using wire::common::Identifier;

[[nodiscard]] const Identifier& frame() {
    static const Identifier block = Identifier::vanilla("end_portal_frame");
    return block;
}

/// The default state of a block that is only ever placed in it.
[[nodiscard]] std::int32_t stateOf(std::string_view path) {
    const registry::BlockState* block = registry::blocks::named(path);
    return block == nullptr ? registry::blocks::air().stateId : block->stateId;
}

[[nodiscard]] std::int32_t endPortal() {
    static const std::int32_t stateId = stateOf("end_portal");
    return stateId;
}

[[nodiscard]] std::int32_t obsidian() {
    static const std::int32_t stateId = stateOf("obsidian");
    return stateId;
}

/// Where a frame sits around a centre, and which way it has to look.
struct Slot {
    std::int32_t x = 0;
    std::int32_t z = 0;
    std::string_view facing;
};

/// The twelve of them, in the order the Kotlin built them: `completeRing` tries each as the offset
/// from the block it was asked about, so the order picks the ring when two of them would fit.
constexpr std::array<Slot, 12> kRing{{
    {-1, -2, "south"},
    {-1, 2, "north"},
    {-2, -1, "east"},
    {2, -1, "west"},
    {0, -2, "south"},
    {0, 2, "north"},
    {-2, 0, "east"},
    {2, 0, "west"},
    {1, -2, "south"},
    {1, 2, "north"},
    {-2, 1, "east"},
    {2, 1, "west"},
}};

[[nodiscard]] std::string propertyOf(const registry::BlockState& block, const std::string& name) {
    const auto found = block.properties.find(name);
    return found == block.properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool isRingComplete(const WorldState& world, std::int32_t centreX, std::int32_t y, std::int32_t centreZ) {
    for (const Slot& slot : kRing) {
        const std::optional<std::int32_t> stateId = world.blockState(centreX + slot.x, y, centreZ + slot.z);
        if (!stateId.has_value()) {
            return false;
        }
        const std::optional<registry::BlockState> block = registry::blocks::byStateId(*stateId);
        if (!block.has_value()) {
            return false;
        }
        if (block->block != frame() || propertyOf(*block, "eye") != "true" ||
            propertyOf(*block, "facing") != slot.facing) {
            return false;
        }
    }
    return true;
}

} // namespace

bool isFrame(std::int32_t stateId) {
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(stateId);
    return block.has_value() && block->block == frame();
}

bool isEndPortal(std::int32_t stateId) { return stateId == endPortal(); }

bool needsEye(std::int32_t stateId) {
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(stateId);
    if (!block.has_value()) {
        return false;
    }
    return block->block == frame() && propertyOf(*block, "eye") != "true";
}

std::vector<BlockChange> placeEye(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !needsEye(*stateId)) {
        return {};
    }
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(*stateId);
    std::string facing = block.has_value() ? propertyOf(*block, "facing") : std::string{};
    if (facing.empty()) {
        facing = "north";
    }
    const std::optional<std::int32_t> armed =
        registry::blocks::stateIdOf(frame(), {{"eye", "true"}, {"facing", facing}});
    if (!armed.has_value()) {
        return {};
    }
    world.setBlockState(x, y, z, *armed);
    std::vector<BlockChange> changed{BlockChange{x, y, z, *armed, {}}};
    std::vector<BlockChange> filled = completeRing(world, x, y, z);
    changed.insert(changed.end(), filled.begin(), filled.end());
    return changed;
}

std::vector<BlockChange> completeRing(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (const Slot& slot : kRing) {
        const std::int32_t centreX = x - slot.x;
        const std::int32_t centreZ = z - slot.z;
        if (!isRingComplete(world, centreX, y, centreZ)) {
            continue;
        }
        std::vector<BlockChange> filled;
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                const std::int32_t blockX = centreX + offsetX;
                const std::int32_t blockZ = centreZ + offsetZ;
                world.setBlockState(blockX, y, blockZ, endPortal());
                filled.push_back(BlockChange{blockX, y, blockZ, endPortal(), {}});
            }
        }
        return filled;
    }
    return {};
}

void createEndPlatform(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (std::int32_t offsetX = -2; offsetX <= 2; ++offsetX) {
        for (std::int32_t offsetZ = -2; offsetZ <= 2; ++offsetZ) {
            world.getOrGenerateChunk((x + offsetX) >> 4, (z + offsetZ) >> 4);
            for (std::int32_t offsetY = -1; offsetY < 3; ++offsetY) {
                const std::int32_t stateId = offsetY == -1 ? obsidian() : registry::blocks::air().stateId;
                world.setBlockState(x + offsetX, y + offsetY, z + offsetZ, stateId);
            }
        }
    }
}

Exit exitFor(WorldState& to, bool fromEnd, double spawnX, double spawnY, double spawnZ) {
    if (fromEnd) {
        return Exit{spawnX, spawnY, spawnZ};
    }
    createEndPlatform(to, kSpawnX, kSpawnY, kSpawnZ);
    return Exit{kSpawnX + 0.5, static_cast<double>(kSpawnY), kSpawnZ + 0.5};
}

} // namespace basalt::server::runtime::world::endPortals
