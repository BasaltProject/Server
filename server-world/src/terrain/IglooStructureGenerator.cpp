#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/TemplateBlockProcessors.hpp>
#include <basalt/server/world/terrain/IglooStructureGenerator.hpp>

#include <array>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kGenerationHeight = 90;
constexpr double kBasementChance = 0.5;

constexpr std::string_view kTop = "igloo/top";
constexpr std::string_view kLadder = "igloo/middle";
constexpr std::string_view kLaboratory = "igloo/bottom";

[[nodiscard]] StructurePos pivotOf(std::string_view name) {
    if (name == kLadder) {
        return StructurePos{1, 3, 1};
    }
    return name == kLaboratory ? StructurePos{3, 6, 7} : StructurePos{3, 5, 5};
}

[[nodiscard]] StructurePos offsetOf(std::string_view name) {
    if (name == kLadder) {
        return StructurePos{2, -3, 4};
    }
    return name == kLaboratory ? StructurePos{0, -3, -2} : StructurePos{0, 0, 0};
}

[[nodiscard]] std::int32_t snowBlock() {
    static const std::int32_t stateId = [] {
        const registry::BlockState* found = registry::blocks::named("snow_block");
        return found == nullptr ? registry::blocks::air().stateId : found->stateId;
    }();
    return stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] StructurePos plus(StructurePos first, StructurePos second) {
    return StructurePos{first.x + second.x, first.y + second.y, first.z + second.z};
}

} // namespace

IglooStructureGenerator::IglooStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> iglooBiomes,
                                                 TemplateOf templateOf, SurfaceHeightAt surfaceHeightAt,
                                                 WorldHeightAt worldHeightAt)
    : biomeAt_(std::move(biomeAt)), iglooBiomes_(std::move(iglooBiomes)), templateOf_(std::move(templateOf)),
      surfaceHeightAt_(std::move(surfaceHeightAt)), worldHeightAt_(std::move(worldHeightAt)) {}

const structure::StructurePlacementGrid& IglooStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{32, 8, 14357618};
    return built;
}

void IglooStructureGenerator::placeChestMarker(Chunk& chunk, const structure::StructureTemplate& piece,
                                               StructurePos position, StructureRotation rotation, StructurePos pivot,
                                               const StructureBoundingBox& clip, random::RandomSource& random) {
    for (const structure::StructureDataMarker& marker :
         piece.dataMarkers(position, rotation, StructureMirror::None, pivot)) {
        if (marker.metadata != "chest" || !clip.isInside(marker.pos)) {
            continue;
        }
        chunk.setBlockStateId(marker.pos.x & 15, marker.pos.y, marker.pos.z & 15, registry::blocks::air().stateId);
        const StructurePos below{marker.pos.x, marker.pos.y - 1, marker.pos.z};
        if (pathOf(chunk.blockStateId(below.x & 15, below.y, below.z & 15)) != "chest") {
            continue;
        }
        wire::nbt::NbtCompound data;
        data.set("LootTable", wire::nbt::NbtTag::ofString("minecraft:chests/igloo_chest"));
        data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(random.nextLong()));
        data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
        chunk.setBlockEntity(ChunkBlockEntity{below.x & 15, below.y, below.z & 15,
                                              wire::common::Identifier::vanilla("chest"), std::move(data)});
    }
}

void IglooStructureGenerator::closeTrapDoorWithoutShaft(Chunk& chunk, StructurePos position, StructureRotation rotation,
                                                        StructurePos pivot, const StructureBoundingBox& clip) {
    const StructurePos trapDoor =
        plus(structure::transform(StructurePos{3, 0, 5}, StructureMirror::None, rotation, pivot), position);
    if (!clip.isInside(trapDoor)) {
        return;
    }
    const std::int32_t below = chunk.decorationBlockStateId(trapDoor.x, trapDoor.y - 1, trapDoor.z);
    if (registry::blocks::isAir(below) || pathOf(below) == "ladder") {
        return;
    }
    chunk.setBlockStateId(trapDoor.x & 15, trapDoor.y, trapDoor.z & 15, snowBlock());
}

std::optional<StructureBoundingBox> IglooStructureGenerator::place(Chunk& chunk, const StructureBoundingBox& clip,
                                                                   random::RandomSource& random, std::string_view name,
                                                                   StructurePos start, StructureRotation rotation,
                                                                   std::int32_t depth) const {
    const structure::StructureTemplate* found = templateOf_(name);
    if (found == nullptr) {
        return std::nullopt;
    }
    const StructurePos pivot = pivotOf(name);
    const StructurePos offset = offsetOf(name);
    const StructurePos placed{start.x + offset.x, start.y + offset.y - depth, start.z + offset.z};

    // parity: the whole igloo settles on the surface at its entrance column, not its corner.
    const StructurePos entrance = plus(
        structure::transform(StructurePos{3 - offset.x, 0, -offset.z}, StructureMirror::None, rotation, pivot), placed);
    const std::int32_t surface = worldHeightAt_(entrance.x, entrance.z, HeightmapType::WorldSurface);
    const StructurePos settled{placed.x, placed.y + surface - kGenerationHeight - 1, placed.z};

    found->placeInto(chunk, structure::TemplatePlacement{.position = settled,
                                                         .rotation = rotation,
                                                         .mirror = StructureMirror::None,
                                                         .pivot = pivot,
                                                         .clipTo = clip,
                                                         .blockProcessor =
                                                             structure::templateBlockProcessors::ignoreStructureBlock(),
                                                         .random = &random});
    placeChestMarker(chunk, *found, settled, rotation, pivot, clip, random);
    if (name == kTop) {
        closeTrapDoorWithoutShaft(chunk, settled, rotation, pivot, clip);
    }
    return found->getBoundingBox(settled, rotation, pivot, StructureMirror::None);
}

void IglooStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t chunkX = chunk.chunkX();
    const std::int32_t chunkZ = chunk.chunkZ();
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    if (!iglooBiomes_.contains(biomeAt_(centreX, surfaceHeightAt_(centreX, centreZ) - 1, centreZ))) {
        return;
    }

    random::RandomSource pick = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(pick, seed, chunkX, chunkZ);
    const StructurePos start{chunkX * 16, kGenerationHeight, chunkZ * 16};
    static constexpr std::array<StructureRotation, 4> kRotations{
        StructureRotation::None, StructureRotation::Clockwise90, StructureRotation::Clockwise180,
        StructureRotation::Counterclockwise90};
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(pick.nextInt(static_cast<std::int32_t>(kRotations.size())))];

    // parity: the basement is decided first and its pieces are added before the igloo itself, which
    // is the order they are then written in.
    std::vector<std::pair<std::string_view, std::int32_t>> pieces;
    if (pick.nextDouble() < kBasementChance) {
        const std::int32_t depth = pick.nextInt(8) + 4;
        pieces.emplace_back(kLaboratory, depth * 3);
        for (std::int32_t level = 0; level < depth - 1; ++level) {
            pieces.emplace_back(kLadder, level * 3);
        }
    }
    pieces.emplace_back(kTop, 0);

    const StructureBoundingBox clip{chunkX * 16,      kOverworldHeight.minY(), chunkZ * 16,
                                    chunkX * 16 + 15, kOverworldHeight.maxY(), chunkZ * 16 + 15};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed = random::setDecorationSeed(random, seed, chunkX * 16, chunkZ * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);

    std::vector<StructureBoundingBox> boxes;
    for (const auto& [name, depth] : pieces) {
        const std::optional<StructureBoundingBox> box = place(chunk, clip, random, name, start, rotation, depth);
        if (box.has_value()) {
            boxes.push_back(*box);
        }
    }
    if (boxes.empty()) {
        return;
    }
    const std::optional<structure::StructureStartRecord> whole =
        structure::StructureStartRecord::of(std::string{kName}, chunkX, chunkZ, boxes);
    if (!whole.has_value()) {
        return;
    }
    std::vector<StructureBoundingBox> here;
    for (const StructureBoundingBox& box : boxes) {
        if (box.intersects(clip)) {
            here.push_back(box);
        }
    }
    if (!here.empty()) {
        chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                               .chunkX = chunkX,
                                                               .chunkZ = chunkZ,
                                                               .fullBounds = whole->fullBounds,
                                                               .pieces = std::move(here)});
    }
}

} // namespace basalt::server::world::terrain
