#include "LightSpread.hpp"

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_map>

namespace basalt::server::world::light {

namespace skyLight {
namespace {

[[nodiscard]] std::int32_t uncachedOpacity(std::int32_t stateId) { return registry::blockShapes::dampening(stateId); }

/// One answer per block state, worked out once.
///
/// The chain behind it costs a lookup and a handful of calls, and a light pass asks it a few hundred
/// thousand times: as a table it measured two orders of magnitude cheaper, which is the same reason
/// the heightmaps have one.
[[nodiscard]] const std::vector<std::int32_t>& table() {
    static const std::vector<std::int32_t> built = [] {
        std::vector<std::int32_t> table(static_cast<std::size_t>(registry::blocks::stateCount()));
        for (std::size_t stateId = 0; stateId < table.size(); ++stateId) {
            table[stateId] = uncachedOpacity(static_cast<std::int32_t>(stateId));
        }
        return table;
    }();
    return built;
}

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kSides = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

} // namespace

std::int32_t opacity(std::int32_t stateId) {
    const std::vector<std::int32_t>& built = table();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= built.size()) {
        return uncachedOpacity(stateId);
    }
    return built[static_cast<std::size_t>(stateId)];
}

std::span<const std::int32_t> opacityTable() { return table(); }

std::vector<SectionLight> compute(const LightWindow& window) {
    const auto [lowestSurface, highestSurface] = window.surfaceRange();
    const std::int32_t topY = std::clamp(highestSurface + 1, window.minY(), window.maxY());
    const std::int32_t bottomY = std::clamp(lowestSurface - kMax - 1, window.minY(), topY);
    LightSpread pass{window, bottomY, topY, kMax};

    // parity: `ChunkSkyLightSources`: a column is a source at full strength down to the first block
    // that dampens light at all. Everything above that floor is simply fifteen.
    std::vector<std::int32_t> floor(static_cast<std::size_t>(LightWindow::kWidth) * LightWindow::kWidth);
    for (std::int32_t z = 0; z < LightWindow::kWidth; ++z) {
        for (std::int32_t x = 0; x < LightWindow::kWidth; ++x) {
            std::int32_t lowest = topY + 1;
            for (std::int32_t y = topY; y >= bottomY; --y) {
                if (pass.opacityAt(x, y, z) > 0) {
                    break;
                }
                pass.set(x, y, z, kMax);
                lowest = y;
            }
            floor[static_cast<std::size_t>(z * LightWindow::kWidth + x)] = lowest;
        }
    }

    // Only the edge of that lit region goes in the queue. A block of open sky with open sky all
    // round it has nothing to give anyone, and queueing it was most of the cost of a light pass.
    for (std::int32_t z = 0; z < LightWindow::kWidth; ++z) {
        for (std::int32_t x = 0; x < LightWindow::kWidth; ++x) {
            const std::int32_t lowest = floor[static_cast<std::size_t>(z * LightWindow::kWidth + x)];
            if (lowest > topY) {
                continue;
            }
            std::int32_t highestNeighbourFloor = lowest;
            for (const auto& [stepX, stepZ] : kSides) {
                const std::int32_t nextX = x + stepX;
                const std::int32_t nextZ = z + stepZ;
                if (nextX < 0 || nextX >= LightWindow::kWidth || nextZ < 0 || nextZ >= LightWindow::kWidth) {
                    continue;
                }
                highestNeighbourFloor = std::max(highestNeighbourFloor,
                                                 floor[static_cast<std::size_t>(nextZ * LightWindow::kWidth + nextX)]);
            }
            for (std::int32_t y = lowest; y <= std::min(highestNeighbourFloor, topY); ++y) {
                pass.push(x, y, z, kMax);
            }
        }
    }
    return pass.run();
}

} // namespace skyLight

namespace blockLight {
namespace {

/// parity: `BlockBehaviour.Properties.lightLevel`, by block rather than by state: the report the
/// other tables come from does not carry light levels.
[[nodiscard]] const std::vector<std::int32_t>& table() {
    static const std::vector<std::int32_t> built = [] {
        const std::array<std::pair<std::int32_t, std::vector<std::string_view>>, 11> emission = {{
            {15,
             {"beacon", "conduit", "end_gateway", "end_portal", "fire", "glowstone", "jack_o_lantern", "lantern",
              "lava", "lava_cauldron", "sea_lantern", "shroomlight", "campfire", "ochre_froglight",
              "pearlescent_froglight", "verdant_froglight", "respawn_anchor"}},
            {14, {"torch", "wall_torch", "end_rod"}},
            {13, {"amethyst_cluster"}},
            {11, {"nether_portal", "crying_obsidian"}},
            {10, {"soul_torch", "soul_wall_torch", "soul_lantern", "soul_fire", "soul_campfire"}},
            {9, {"deepslate_redstone_ore", "redstone_ore"}},
            {7, {"enchanting_table", "ender_chest", "glow_lichen", "redstone_torch", "large_amethyst_bud"}},
            {6, {"sculk_catalyst", "medium_amethyst_bud"}},
            {4, {"small_amethyst_bud"}},
            {3, {"magma_block", "sculk_sensor", "sculk_shrieker"}},
            {1, {"brewing_stand", "brown_mushroom", "dragon_egg", "sculk_vein"}},
        }};
        std::vector<std::int32_t> table(static_cast<std::size_t>(registry::blocks::stateCount()), 0);
        for (const auto& [level, names] : emission) {
            for (const std::string_view name : names) {
                const std::optional<std::int32_t> stateId =
                    registry::blocks::registry().idOf(wire::common::Identifier::vanilla(std::string{name}));
                if (stateId.has_value() && static_cast<std::size_t>(*stateId) < table.size()) {
                    table[static_cast<std::size_t>(*stateId)] = level;
                }
            }
        }
        return table;
    }();
    return built;
}

} // namespace

std::int32_t emission(std::int32_t stateId) {
    const std::vector<std::int32_t>& built = table();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= built.size()) {
        return 0;
    }
    return built[static_cast<std::size_t>(stateId)];
}

std::optional<std::vector<SectionLight>> compute(const LightWindow& window) {
    const std::optional<std::pair<std::size_t, std::size_t>> sections = window.emitterSections(emission);
    if (!sections.has_value()) {
        return std::nullopt;
    }
    const auto [first, last] = *sections;
    const std::int32_t bottomY = std::max(window.minY() + (static_cast<std::int32_t>(first) - 1) * 16, window.minY());
    const std::int32_t topY = std::min(window.minY() + (static_cast<std::int32_t>(last) + 2) * 16 - 1, window.maxY());
    LightSpread pass{window, bottomY, topY, 0};
    for (std::int32_t z = 0; z < LightWindow::kWidth; ++z) {
        for (std::int32_t x = 0; x < LightWindow::kWidth; ++x) {
            for (std::int32_t y = bottomY; y <= topY; ++y) {
                const std::int32_t emitted = emission(pass.stateAt(x, y, z));
                if (emitted > 0) {
                    pass.seed(x, y, z, emitted);
                }
            }
        }
    }
    return pass.run();
}

} // namespace blockLight

} // namespace basalt::server::world::light
