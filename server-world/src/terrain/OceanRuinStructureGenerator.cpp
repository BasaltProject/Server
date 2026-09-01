#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/TemplateBlockProcessors.hpp>
#include <basalt/server/world/terrain/OceanRuinStructureGenerator.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr float kLargeProbability = 0.3F;
constexpr float kClusterProbability = 0.9F;

/// parity: the piece is built at y 90 and settled afterwards.
constexpr std::int32_t kPlaceholderY = 90;
constexpr std::int32_t kReachChunks = 3;

constexpr StructurePos kZero{0, 0, 0};

constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

/// The eight small lists are numbered one to eight; the large ones are 1, 2, 3 and 8, because the
/// jar skips 4 to 7 for the cold ruins, and 4 to 7 for the warm ones.
[[nodiscard]] std::string ruin(std::string_view prefix, std::int32_t index) {
    return "underwater_ruin/" + std::string{prefix} + "_" + std::to_string(index);
}

[[nodiscard]] std::string smallRuin(std::string_view prefix, std::int32_t index) { return ruin(prefix, index + 1); }

[[nodiscard]] std::string bigColdRuin(std::string_view prefix, std::int32_t index) {
    static constexpr std::array<std::int32_t, 4> kNumbers{1, 2, 3, 8};
    return ruin(prefix, kNumbers[static_cast<std::size_t>(index)]);
}

[[nodiscard]] std::string bigWarmRuin(std::int32_t index) { return ruin("big_warm", index + 4); }

[[nodiscard]] bool isIce(std::string_view path) {
    return path == "ice" || path == "packed_ice" || path == "blue_ice" || path == "frosted_ice";
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] bool isOpen(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t stateId = chunk.decorationBlockStateId(x, y, z);
    if (registry::blocks::isAir(stateId)) {
        return true;
    }
    const std::string path = pathOf(stateId);
    return path == "water" || isIce(path);
}

/// parity: `Mth.nextInt(random, min, max)`, inclusive at both ends.
[[nodiscard]] std::int32_t between(random::RandomSource& random, std::int32_t min, std::int32_t max) {
    return random.nextInt(max - min + 1) + min;
}

[[nodiscard]] StructurePos plus(StructurePos first, StructurePos second) {
    return StructurePos{first.x + second.x, first.y + second.y, first.z + second.z};
}

[[nodiscard]] StructurePos offset(StructurePos pos, std::int32_t dx, std::int32_t dz) {
    return StructurePos{pos.x + dx, pos.y, pos.z + dz};
}

/// parity: `allPositions`, eight spots each drawn from its own pair of ranges.
[[nodiscard]] std::vector<StructurePos> allPositions(random::RandomSource& random, StructurePos origin) {
    std::vector<StructurePos> spots;
    spots.reserve(8);
    spots.push_back(offset(origin, -16 + between(random, 1, 8), 16 + between(random, 1, 7)));
    spots.push_back(offset(origin, -16 + between(random, 1, 8), between(random, 1, 7)));
    spots.push_back(offset(origin, -16 + between(random, 1, 8), -16 + between(random, 4, 8)));
    spots.push_back(offset(origin, between(random, 1, 7), 16 + between(random, 1, 7)));
    spots.push_back(offset(origin, between(random, 1, 7), -16 + between(random, 4, 6)));
    spots.push_back(offset(origin, 16 + between(random, 1, 7), 16 + between(random, 3, 8)));
    spots.push_back(offset(origin, 16 + between(random, 1, 7), between(random, 1, 7)));
    spots.push_back(offset(origin, 16 + between(random, 1, 7), -16 + between(random, 4, 8)));
    return spots;
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

} // namespace

OceanRuinStructureGenerator::OceanRuinStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> coldBiomes,
                                                         std::set<std::string, std::less<>> warmBiomes,
                                                         TemplateOf templateOf, OceanFloorAt oceanFloorAt)
    : biomeAt_(std::move(biomeAt)), coldBiomes_(std::move(coldBiomes)), warmBiomes_(std::move(warmBiomes)),
      templateOf_(std::move(templateOf)), oceanFloorAt_(std::move(oceanFloorAt)) {}

const structure::StructurePlacementGrid& OceanRuinStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{20, 8, 14357621};
    return built;
}

std::optional<bool> OceanRuinStructureGenerator::chooseVariant(std::int64_t seed, std::int32_t chunkX,
                                                               std::int32_t chunkZ) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    const std::int32_t floor = oceanFloorAt_(centreX, centreZ) - 1;
    // false is `ocean_ruin_cold`, true is `ocean_ruin_warm`, in the set's own order.
    std::vector<bool> options{false, true};
    while (!options.empty()) {
        const std::size_t index = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(options.size())));
        const bool warm = options[index];
        const std::string biome = biomeAt_(centreX, floor, centreZ);
        if (warm ? warmBiomes_.contains(biome) : coldBiomes_.contains(biome)) {
            return !warm;
        }
        options.erase(options.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return std::nullopt;
}

StructurePos OceanRuinStructureGenerator::settle(const Chunk& chunk, const structure::StructureTemplate& piece,
                                                 StructurePos position, StructureRotation rotation) const {
    const std::int32_t floor = oceanFloorAt_(position.x, position.z);
    const StructurePos start{position.x, floor, position.z};
    const StructurePos corner = plus(structure::transform(StructurePos{piece.size().x - 1, 0, piece.size().z - 1},
                                                          StructureMirror::None, rotation, kZero),
                                     start);
    std::int32_t lowest = 512;
    std::int32_t hollow = 0;
    const std::int32_t topY = start.y - 1;
    for (std::int32_t x = std::min(start.x, corner.x); x <= std::max(start.x, corner.x); ++x) {
        for (std::int32_t z = std::min(start.z, corner.z); z <= std::max(start.z, corner.z); ++z) {
            std::int32_t y = start.y - 1;
            while (y > kOverworldHeight.minY() + 1 && isOpen(chunk, x, y, z)) {
                --y;
            }
            lowest = std::min(lowest, y);
            if (y < topY - 2) {
                ++hollow;
            }
        }
    }
    const std::int32_t width = std::abs(start.x - corner.x);
    const std::int32_t settledY = (topY - lowest > 2 && hollow > width - 2) ? lowest + 1 : start.y;
    return StructurePos{position.x, settledY, position.z};
}

std::vector<StructureBoundingBox>
OceanRuinStructureGenerator::addPiece(Chunk& chunk, const StructureBoundingBox& clip, random::RandomSource& random,
                                      random::RandomSource& pick, StructurePos position, StructureRotation rotation,
                                      bool cold, bool large, float baseIntegrity) const {
    std::vector<std::pair<std::string, float>> names;
    if (cold) {
        const std::int32_t count = large ? 4 : 8;
        const std::int32_t index = pick.nextInt(count);
        names.emplace_back(large ? bigColdRuin("big_brick", index) : smallRuin("brick", index), baseIntegrity);
        names.emplace_back(large ? bigColdRuin("big_cracked", index) : smallRuin("cracked", index), 0.7F);
        names.emplace_back(large ? bigColdRuin("big_mossy", index) : smallRuin("mossy", index), 0.5F);
    } else {
        const std::int32_t index = pick.nextInt(large ? 4 : 8);
        names.emplace_back(large ? bigWarmRuin(index) : smallRuin("warm", index), baseIntegrity);
    }

    std::vector<StructureBoundingBox> boxes;
    for (const auto& [name, integrity] : names) {
        const structure::StructureTemplate* piece = templateOf_(name);
        if (piece == nullptr) {
            continue;
        }
        const StructurePos settled = settle(chunk, *piece, position, rotation);
        const structure::StructureProcessorList processors{{}, structure::BlockRot{.integrity = integrity}};
        piece->placeInto(chunk, structure::TemplatePlacement{
                                    .position = settled,
                                    .rotation = rotation,
                                    .mirror = StructureMirror::None,
                                    .pivot = kZero,
                                    .clipTo = clip,
                                    .processors = &processors,
                                    .blockProcessor = structure::templateBlockProcessors::ignoreStructureAndAir(),
                                    .random = &random,
                                    .settingsRandom = &random});
        boxes.push_back(piece->getBoundingBox(settled, rotation, kZero, StructureMirror::None));
    }
    return boxes;
}

std::vector<StructureBoundingBox>
OceanRuinStructureGenerator::addCluster(Chunk& chunk, const StructureBoundingBox& clip, random::RandomSource& random,
                                        random::RandomSource& pick, StructurePos parent,
                                        StructureRotation parentRotation, bool cold) const {
    const StructurePos parentOrigin{parent.x, kPlaceholderY, parent.z};
    const StructurePos parentCorner =
        plus(structure::transform(StructurePos{15, 0, 15}, StructureMirror::None, parentRotation, kZero), parentOrigin);
    const StructureBoundingBox parentBox = StructureBoundingBox::fromCorners(parentOrigin, parentCorner);
    const StructurePos bottomLeft{std::min(parentOrigin.x, parentCorner.x), parentOrigin.y,
                                  std::min(parentOrigin.z, parentCorner.z)};
    std::vector<StructurePos> spots = allPositions(pick, bottomLeft);
    const std::int32_t count = between(pick, 4, 8);
    std::vector<StructureBoundingBox> boxes;
    for (std::int32_t taken = 0; taken < count; ++taken) {
        if (spots.empty()) {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(spots.size())));
        const StructurePos spot = spots[index];
        spots.erase(spots.begin() + static_cast<std::ptrdiff_t>(index));
        const StructureRotation rotation =
            kRotations[static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(kRotations.size())))];
        const StructurePos corner =
            plus(structure::transform(StructurePos{5, 0, 6}, StructureMirror::None, rotation, kZero), spot);
        if (StructureBoundingBox::fromCorners(spot, corner).intersects(parentBox)) {
            continue;
        }
        const std::vector<StructureBoundingBox> placed =
            addPiece(chunk, clip, random, pick, spot, rotation, cold, false, 0.8F);
        boxes.insert(boxes.end(), placed.begin(), placed.end());
    }
    return boxes;
}

void OceanRuinStructureGenerator::placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX,
                                            std::int32_t chunkZ) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }
    const std::optional<bool> cold = chooseVariant(seed, chunkX, chunkZ);
    if (!cold.has_value()) {
        return;
    }

    random::RandomSource pick = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(pick, seed, chunkX, chunkZ);
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const bool large = pick.nextFloat() <= kLargeProbability;
    const float baseIntegrity = large ? 0.9F : 0.8F;
    const StructurePos origin{chunkX * 16, kPlaceholderY, chunkZ * 16};

    const StructureBoundingBox clip{chunk.chunkX() * 16,      kOverworldHeight.minY(), chunk.chunkZ() * 16,
                                    chunk.chunkX() * 16 + 15, kOverworldHeight.maxY(), chunk.chunkZ() * 16 + 15};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, *cold ? kColdStructureIndex : kWarmStructureIndex,
                           kSurfaceStructuresStep);

    std::vector<StructureBoundingBox> boxes =
        addPiece(chunk, clip, random, pick, origin, rotation, *cold, large, baseIntegrity);
    if (large && pick.nextFloat() <= kClusterProbability) {
        const std::vector<StructureBoundingBox> cluster =
            addCluster(chunk, clip, random, pick, origin, rotation, *cold);
        boxes.insert(boxes.end(), cluster.begin(), cluster.end());
    }
    std::vector<StructureBoundingBox> touching;
    for (const StructureBoundingBox& box : boxes) {
        if (box.intersects(clip)) {
            touching.push_back(box);
        }
    }
    if (touching.empty()) {
        return;
    }
    const std::optional<structure::StructureStartRecord> whole =
        structure::StructureStartRecord::of(std::string{kName}, chunkX, chunkZ, boxes);
    if (!whole.has_value()) {
        return;
    }
    chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                           .chunkX = chunkX,
                                                           .chunkZ = chunkZ,
                                                           .fullBounds = whole->fullBounds,
                                                           .pieces = std::move(touching)});
}

void OceanRuinStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t spacing = grid().spacing();
    const std::int32_t cellX = floorDiv(chunk.chunkX(), spacing);
    const std::int32_t cellZ = floorDiv(chunk.chunkZ(), spacing);
    for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
        for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            const auto [candidateX, candidateZ] =
                grid().candidateChunk(seed, (cellX + offsetX) * spacing, (cellZ + offsetZ) * spacing);
            if (std::abs(candidateX - chunk.chunkX()) > kReachChunks ||
                std::abs(candidateZ - chunk.chunkZ()) > kReachChunks) {
                continue;
            }
            placeFrom(chunk, seed, candidateX, candidateZ);
        }
    }
}

} // namespace basalt::server::world::terrain
