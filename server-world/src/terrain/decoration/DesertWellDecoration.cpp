#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureProcessor.hpp>
#include <basalt/server/world/terrain/decoration/DesertWellDecoration.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kGlobalIndex = 2;

/// parity: `Rotation.values()`, the order `TemplateFeature` draws its rotation from.
constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

/// parity: the suspicious sand's five spots, the centre first and then west, south, east and north.
constexpr std::array<std::pair<std::int32_t, std::int32_t>, 5> kSandOffsets{{{0, 0}, {-1, 0}, {0, 1}, {1, 0}, {0, -1}}};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
    }
    return found->stateId;
}

[[nodiscard]] std::int32_t get(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    return chunk.decorationBlockStateId(x, y, z);
}

/// parity: the suspicious sand templates keep the state and gain the well's archaeology loot table.
const structure::StructureProcessorList& suspiciousSandProcessors() {
    static const structure::StructureProcessorList list{{structure::StructureProcessorRule{
        .inputTest = structure::StructureRuleTest::blockMatch(stateIdOf("suspicious_sand")),
        .outputStateId = stateIdOf("suspicious_sand"),
        .appendLootTable = "minecraft:archaeology/desert_well"}}};
    return list;
}

/// parity: `Rotation.rotate` of a horizontal direction, as a step.
[[nodiscard]] std::pair<std::int32_t, std::int32_t> rotateStep(std::pair<std::int32_t, std::int32_t> step,
                                                               StructureRotation rotation) {
    const auto [dx, dz] = step;
    switch (rotation) {
    case StructureRotation::None:
        return {dx, dz};
    case StructureRotation::Clockwise90:
        return {-dz, dx};
    case StructureRotation::Clockwise180:
        return {-dx, -dz};
    case StructureRotation::Counterclockwise90:
        return {dz, -dx};
    }
    return {dx, dz};
}

/// parity: `TemplateFeature` centres the template by half its size along the rotated west and north.
void placeTemplate(Chunk& chunk, random::RandomSource& random, StructurePos origin, std::string_view name,
                   const structure::TemplateLoader& templates, const structure::StructureProcessorList* processors) {
    static_cast<void>(random.nextInt(1));
    const StructureRotation rotation = kRotations[static_cast<std::size_t>(random.nextInt(4))];
    const structure::StructureTemplate* found = templates(wire::common::Identifier::vanilla(std::string{name}));
    if (found == nullptr) {
        return;
    }
    const auto [westX, westZ] = rotateStep({-1, 0}, rotation);
    const auto [northX, northZ] = rotateStep({0, -1}, rotation);
    const StructurePos size = found->size();
    const StructurePos position{origin.x + westX * (size.x / 2) + northX * (size.z / 2), origin.y,
                                origin.z + westZ * (size.x / 2) + northZ * (size.z / 2)};
    const ChunkHeight bounds = chunk.verticalBounds();
    structure::TemplatePlacement placement;
    placement.position = position;
    placement.rotation = rotation;
    placement.pivot = StructurePos{0, 0, 0};
    // parity: a feature writes as far as the region reaches, the chunk and one ring around it.
    placement.clipTo =
        structure::StructureBoundingBox{chunk.chunkX() * 16 - 16, bounds.minY(), chunk.chunkZ() * 16 - 16,
                                        chunk.chunkX() * 16 + 31, bounds.maxY(), chunk.chunkZ() * 16 + 31};
    placement.processors = processors;
    placement.settingsRandom = &random;
    found->placeInto(chunk, placement);
}

} // namespace

void desertWellDecoration::placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y,
                                   std::int32_t z, const structure::TemplateLoader& templates) {
    if (!templates) {
        return;
    }
    // parity: the 26.3 `desert_well`.
    placeTemplate(chunk, random, StructurePos{x, y - 2, z}, "desert_well/well", templates, nullptr);
    for (const std::int32_t depth : {1, 2}) {
        const auto [dx, dz] = kSandOffsets[static_cast<std::size_t>(random.nextInt(kSandOffsets.size()))];
        placeTemplate(chunk, random, StructurePos{x + dx, y - depth, z + dz}, "desert_well/suspicious_sand", templates,
                      &suspiciousSandProcessors());
    }
}

void desertWellDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                    const structure::TemplateLoader& templates) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kGlobalIndex, kSurfaceStructuresStep);
    if (random.nextFloat() >= 1.0F / 1000.0F) {
        return;
    }
    const std::int32_t localX = random.nextInt(16);
    const std::int32_t localZ = random.nextInt(16);
    const std::int32_t height = chunk.heightAt(localX, localZ, HeightmapType::MotionBlocking);
    if (height <= chunk.verticalBounds().minY()) {
        return;
    }
    const std::int32_t x = chunk.chunkX() * 16 + localX;
    const std::int32_t y = height - 1;
    const std::int32_t z = chunk.chunkZ() * 16 + localZ;
    // parity: sand here and solid ground in the 5 by 5 by 2 box under it.
    if (get(chunk, x, y, z) != stateIdOf("sand")) {
        return;
    }
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            for (std::int32_t dy = -2; dy <= -1; ++dy) {
                if (registry::blocks::isAir(get(chunk, x + dx, y + dy, z + dz))) {
                    return;
                }
            }
        }
    }
    const std::string& biome = environment.biomeAt(x, y, z);
    const std::size_t colon = biome.find(':');
    if ((colon == std::string::npos ? biome : biome.substr(colon + 1)) != "desert") {
        return;
    }
    placeAt(chunk, random, x, y, z, templates);
}

} // namespace basalt::server::world::terrain::decoration
