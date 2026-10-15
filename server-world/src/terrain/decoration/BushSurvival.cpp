#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/server/world/light/LightWindow.hpp>
#include <basalt/server/world/terrain/decoration/BushSurvival.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

namespace registry = basalt::server::registry;

constexpr std::int32_t kTooBright = 13;

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] bool isMushroom(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path == "brown_mushroom" || path == "red_mushroom";
}

/// parity: block tag `overrides_mushroom_light_requirement`.
[[nodiscard]] bool overridesLight(std::int32_t stateId) {
    static const std::set<std::string, std::less<>> paths = {"mycelium", "podzol", "crimson_nylium", "warped_nylium"};
    return paths.contains(pathOf(stateId));
}

[[nodiscard]] std::int32_t levelAt(const std::vector<light::SectionLight>& sections, std::size_t section,
                                   std::int32_t localX, std::int32_t y, std::int32_t localZ) {
    if (section >= sections.size()) {
        return 0;
    }
    const light::SectionLight& values = sections[section];
    const auto index = static_cast<std::size_t>(((y & 15) << 8) | ((localZ & 15) << 4) | (localX & 15));
    return index < values.size() ? values[index] : 0;
}

} // namespace

namespace {

/// parity: `DoublePlantBlock.updateShape` turns a half with a mismatched partner into air.
void settleDoublePlants(Chunk& chunk) {
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t air = registry::blocks::air().stateId;
    for (std::size_t section = 0; section < chunk.sectionCount(); ++section) {
        if (!chunk.section(section).mayContain(
                [](std::int32_t stateId) { return registry::blockStateFamilies::isDoublePlantState(stateId); })) {
            continue;
        }
        const std::int32_t firstY = bounds.minY() + static_cast<std::int32_t>(section) * 16;
        for (std::int32_t localX = 0; localX < 16; ++localX) {
            for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
                for (std::int32_t y = firstY; y < firstY + 16 && y <= bounds.maxY(); ++y) {
                    const std::int32_t stateId = chunk.blockStateId(localX, y, localZ);
                    if (!registry::blockStateFamilies::isDoublePlantState(stateId)) {
                        continue;
                    }
                    // The lower half is the one the registry lists, and the upper is one below it.
                    const bool lower = registry::blockStateFamilies::upperDoublePlantStateId(stateId).has_value();
                    const std::int32_t partnerY = lower ? y + 1 : y - 1;
                    const std::int32_t wanted = lower ? stateId - 1 : stateId + 1;
                    if (partnerY < bounds.minY() || partnerY > bounds.maxY() ||
                        chunk.blockStateId(localX, partnerY, localZ) != wanted) {
                        chunk.setBlockStateId(localX, y, localZ, air);
                    }
                }
            }
        }
    }
}

} // namespace

void bushSurvival::settle(Chunk& chunk, const NeighbourAt& neighbourAt, const bool hasSkylight) {
    settleDoublePlants(chunk);
    std::vector<std::size_t> interesting;
    for (std::size_t index = 0; index < chunk.sectionCount(); ++index) {
        if (chunk.section(index).mayContain([](std::int32_t stateId) { return isMushroom(stateId); })) {
            interesting.push_back(index);
        }
    }
    if (interesting.empty()) {
        return;
    }

    const light::LightWindow window{chunk, neighbourAt};
    const std::optional<std::vector<light::SectionLight>> blockLight = light::blockLight::compute(window);
    const std::vector<light::SectionLight> skyLight =
        hasSkylight ? light::skyLight::compute(window) : std::vector<light::SectionLight>{};
    if (!blockLight.has_value() && skyLight.empty()) {
        return;
    }

    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t air = registry::blocks::air().stateId;
    for (const std::size_t section : interesting) {
        const std::int32_t firstY = bounds.minY() + static_cast<std::int32_t>(section) * 16;
        for (std::int32_t localX = 0; localX < 16; ++localX) {
            for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
                for (std::int32_t y = firstY; y < firstY + 16 && y <= bounds.maxY(); ++y) {
                    if (!isMushroom(chunk.blockStateId(localX, y, localZ))) {
                        continue;
                    }
                    if (y > bounds.minY() && overridesLight(chunk.blockStateId(localX, y - 1, localZ))) {
                        continue;
                    }
                    const std::int32_t fromBlocks =
                        blockLight.has_value() ? levelAt(*blockLight, section, localX, y, localZ) : 0;
                    const std::int32_t fromSky = skyLight.empty() ? 0 : levelAt(skyLight, section, localX, y, localZ);
                    if (std::max(fromBlocks, fromSky) >= kTooBright) {
                        chunk.setBlockStateId(localX, y, localZ, air);
                    }
                }
            }
        }
    }
}

} // namespace basalt::server::world::terrain::decoration
