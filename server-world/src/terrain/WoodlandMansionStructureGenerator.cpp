#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/TemplateBlockProcessors.hpp>
#include <basalt/server/world/terrain/WoodlandMansionStructureGenerator.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kReachChunks = 7;
constexpr std::int32_t kMinGround = 60;
constexpr std::string_view kTemplatePrefix = "woodland_mansion/";
constexpr StructurePos kPivot{0, 0, 0};

[[nodiscard]] std::int32_t cobblestone() {
    static const std::int32_t stateId = [] {
        const registry::BlockState* found = registry::blocks::named("cobblestone");
        return found == nullptr ? registry::blocks::air().stateId : found->stateId;
    }();
    return stateId;
}

[[nodiscard]] std::string chestFacing(std::string_view metadata) {
    if (metadata == "ChestSouth") {
        return "south";
    }
    if (metadata == "ChestEast") {
        return "east";
    }
    if (metadata == "ChestWest") {
        return "west";
    }
    return "north";
}

[[nodiscard]] std::string clockwiseOf(const std::string& facing) {
    if (facing == "north") {
        return "east";
    }
    if (facing == "east") {
        return "south";
    }
    return facing == "south" ? "west" : "north";
}

[[nodiscard]] std::string turn(const std::string& facing, StructureRotation rotation) {
    std::string result = facing;
    for (std::int32_t quarter = 0; quarter < static_cast<std::int32_t>(rotation); ++quarter) {
        result = clockwiseOf(result);
    }
    return result;
}

[[nodiscard]] StructureBoundingBox unionOf(const StructureBoundingBox& first, const StructureBoundingBox& second) {
    return StructureBoundingBox{std::min(first.minX, second.minX), std::min(first.minY, second.minY),
                                std::min(first.minZ, second.minZ), std::max(first.maxX, second.maxX),
                                std::max(first.maxY, second.maxY), std::max(first.maxZ, second.maxZ)};
}

[[nodiscard]] StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ) {
    return StructureBoundingBox{chunkX * 16,      kOverworldHeight.minY(), chunkZ * 16,
                                chunkX * 16 + 15, kOverworldHeight.maxY(), chunkZ * 16 + 15};
}

void addResident(Chunk& chunk, StructurePos position, std::string_view type) {
    wire::nbt::NbtCompound data;
    data.set("PersistenceRequired", wire::nbt::NbtTag::ofByte(1));
    chunk.addGeneratedEntity(ChunkGeneratedEntity{.type = wire::common::Identifier::vanilla(std::string{type}),
                                                  .x = position.x + 0.5,
                                                  .y = static_cast<double>(position.y),
                                                  .z = position.z + 0.5,
                                                  .yaw = 0.0F,
                                                  .pitch = 0.0F,
                                                  .data = std::move(data)});
}

/// parity: `WoodlandMansionStructure.afterPlace`. Every column of this chunk that stands under the
/// house is carried down in cobblestone until it meets something solid, so a mansion on a slope
/// grows its own foundation.
void foundation(Chunk& chunk, const StructureBoundingBox& clip, const StructureBoundingBox& bounds,
                const std::vector<StructureBoundingBox>& here) {
    if (here.empty()) {
        return;
    }
    const std::int32_t yStart = bounds.minY;
    if (yStart < kOverworldHeight.minY() || yStart > kOverworldHeight.maxY()) {
        return;
    }
    for (std::int32_t x = clip.minX; x <= clip.maxX; ++x) {
        for (std::int32_t z = clip.minZ; z <= clip.maxZ; ++z) {
            const std::int32_t standing = chunk.blockStateId(x & 15, yStart, z & 15);
            if (registry::blocks::isAir(standing)) {
                continue;
            }
            const StructurePos position{x, yStart, z};
            if (!bounds.isInside(position) ||
                std::none_of(here.begin(), here.end(),
                             [position](const StructureBoundingBox& box) { return box.isInside(position); })) {
                continue;
            }
            for (std::int32_t y = yStart - 1; y > kOverworldHeight.minY(); --y) {
                const std::int32_t below = chunk.blockStateId(x & 15, y, z & 15);
                if (!registry::blocks::isAir(below) && !registry::blockFluids::hasFluid(below)) {
                    break;
                }
                chunk.setBlockStateId(x & 15, y, z & 15, cobblestone());
            }
        }
    }
}

} // namespace

WoodlandMansionStructureGenerator::WoodlandMansionStructureGenerator(BiomeAt biomeAt,
                                                                     std::set<std::string, std::less<>> mansionBiomes,
                                                                     TemplateOf templateOf,
                                                                     SurfaceHeightAt surfaceHeightAt)
    : biomeAt_(std::move(biomeAt)), mansionBiomes_(std::move(mansionBiomes)), templateOf_(std::move(templateOf)),
      surfaceHeightAt_(std::move(surfaceHeightAt)) {}

const structure::StructurePlacementGrid& WoodlandMansionStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{80, 20, 10387319, structure::SpreadType::Triangular};
    return built;
}

std::optional<StructurePos> WoodlandMansionStructureGenerator::lowestCornerOf(std::int32_t chunkX, std::int32_t chunkZ,
                                                                              StructureRotation rotation) const {
    std::int32_t offsetX = 5;
    std::int32_t offsetZ = 5;
    switch (rotation) {
    case StructureRotation::Clockwise90:
        offsetX = -5;
        break;
    case StructureRotation::Clockwise180:
        offsetX = -5;
        offsetZ = -5;
        break;
    case StructureRotation::Counterclockwise90:
        offsetZ = -5;
        break;
    default:
        break;
    }
    const std::int32_t blockX = chunkX * 16 + 7;
    const std::int32_t blockZ = chunkZ * 16 + 7;
    // parity: `getFirstOccupiedHeight` is the base height minus one, the last solid block rather
    // than the first free one above it, which is what puts the mansion's floor on the ground.
    const auto corner = [this](std::int32_t x, std::int32_t z) { return surfaceHeightAt_(x, z) - 1; };
    const std::int32_t lowest =
        std::min(std::min(corner(blockX, blockZ), corner(blockX, blockZ + offsetZ)),
                 std::min(corner(blockX + offsetX, blockZ), corner(blockX + offsetX, blockZ + offsetZ)));
    if (lowest < kMinGround) {
        return std::nullopt;
    }
    return StructurePos{blockX, lowest, blockZ};
}

void WoodlandMansionStructureGenerator::placeMarkers(Chunk& chunk, const structure::StructureTemplate& piece,
                                                     const structure::MansionPiece& mansionPiece,
                                                     const StructureBoundingBox& clip,
                                                     random::RandomSource& random) const {
    for (const structure::StructureDataMarker& marker :
         piece.dataMarkers(mansionPiece.position, mansionPiece.rotation, mansionPiece.mirror, kPivot)) {
        if (!clip.isInside(marker.pos)) {
            continue;
        }
        const std::int32_t localX = marker.pos.x & 15;
        const std::int32_t localZ = marker.pos.z & 15;
        if (marker.metadata.starts_with("Chest")) {
            const std::string turned = turn(chestFacing(marker.metadata), mansionPiece.rotation);
            const std::map<std::string, std::string> properties = {
                {"facing", turned}, {"type", "single"}, {"waterlogged", "false"}};
            const std::optional<std::int32_t> stateId =
                registry::blocks::stateIdOf(wire::common::Identifier::vanilla("chest"), properties);
            if (!stateId.has_value()) {
                continue;
            }
            chunk.setBlockStateId(localX, marker.pos.y, localZ, *stateId);
            wire::nbt::NbtCompound data;
            data.set("LootTable", wire::nbt::NbtTag::ofString("minecraft:chests/woodland_mansion"));
            data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(random.nextLong()));
            data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
            chunk.setBlockEntity(ChunkBlockEntity{localX, marker.pos.y, localZ,
                                                  wire::common::Identifier::vanilla("chest"), std::move(data)});
        } else if (marker.metadata == "Mage" || marker.metadata == "Warrior") {
            chunk.setBlockStateId(localX, marker.pos.y, localZ, registry::blocks::air().stateId);
            addResident(chunk, marker.pos, marker.metadata == "Mage" ? "evoker" : "vindicator");
        } else if (marker.metadata == "Group of Allays") {
            // parity: one to three of them, and the count comes off the level's own random, which no
            // seed reproduces; a positional draw stands in.
            random::RandomSource source = random::RandomSource::legacy(0);
            const std::int32_t count =
                source.forkPositional().at(marker.pos.x, marker.pos.y, marker.pos.z).nextInt(3) + 1;
            chunk.setBlockStateId(localX, marker.pos.y, localZ, registry::blocks::air().stateId);
            for (std::int32_t index = 0; index < count; ++index) {
                addResident(chunk, marker.pos, "allay");
            }
        } else {
            chunk.setBlockStateId(localX, marker.pos.y, localZ, registry::blocks::air().stateId);
        }
    }
}

namespace {

/// parity: the rotation is drawn before the ground is looked at, so a mansion the ground rejects
/// still costs the roll.
[[nodiscard]] StructureRotation drawRotation(random::RandomSource& random) {
    static constexpr std::array<StructureRotation, 4> kRotations{
        StructureRotation::None, StructureRotation::Clockwise90, StructureRotation::Clockwise180,
        StructureRotation::Counterclockwise90};
    return kRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kRotations.size())))];
}

} // namespace

bool WoodlandMansionStructureGenerator::startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return false;
    }
    random::RandomSource startRandom = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(startRandom, seed, chunkX, chunkZ);
    const std::optional<StructurePos> origin = lowestCornerOf(chunkX, chunkZ, drawRotation(startRandom));
    return origin.has_value() && mansionBiomes_.contains(biomeAt_(origin->x, origin->y, origin->z));
}

void WoodlandMansionStructureGenerator::placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX,
                                                  std::int32_t chunkZ) {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }

    random::RandomSource startRandom = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(startRandom, seed, chunkX, chunkZ);

    const StructureRotation rotation = drawRotation(startRandom);
    const std::optional<StructurePos> origin = lowestCornerOf(chunkX, chunkZ, rotation);
    // parity: `Structure.isValidBiome` reads the stub, which for a mansion is that lowest corner.
    if (!origin.has_value() || !mansionBiomes_.contains(biomeAt_(origin->x, origin->y, origin->z))) {
        return;
    }

    std::vector<structure::MansionPiece> pieces;
    const structure::MansionGrid mansion{startRandom};
    structure::mansionPlacer::createMansion(pieces, startRandom, *origin, rotation, mansion);
    if (pieces.empty()) {
        return;
    }

    const StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ());
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);

    std::optional<StructureBoundingBox> whole;
    std::vector<StructureBoundingBox> here;
    const structure::TemplateBlockProcessor ignoreStructureBlock =
        structure::templateBlockProcessors::ignoreStructureBlock();
    for (const structure::MansionPiece& piece : pieces) {
        const structure::StructureTemplate* found = templateOf_(std::string{kTemplatePrefix} + piece.templateName);
        if (found == nullptr) {
            continue;
        }
        const StructureBoundingBox box = found->getBoundingBox(piece.position, piece.rotation, kPivot, piece.mirror);
        whole = whole.has_value() ? unionOf(*whole, box) : box;
        if (!box.intersects(clip)) {
            continue;
        }
        here.push_back(box);
        found->placeInto(chunk, structure::TemplatePlacement{.position = piece.position,
                                                             .rotation = piece.rotation,
                                                             .mirror = piece.mirror,
                                                             .pivot = kPivot,
                                                             .clipTo = clip,
                                                             .blockProcessor = ignoreStructureBlock,
                                                             .random = &random});
        placeMarkers(chunk, *found, piece, clip, random);
    }
    if (!whole.has_value()) {
        return;
    }
    foundation(chunk, clip, *whole, here);
    if (!here.empty()) {
        chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                               .chunkX = chunkX,
                                                               .chunkZ = chunkZ,
                                                               .fullBounds = *whole,
                                                               .pieces = std::move(here)});
    }
}

void WoodlandMansionStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t spacing = grid().spacing();
    const auto floorDiv = [](std::int32_t value, std::int32_t divisor) {
        const std::int32_t quotient = value / divisor;
        return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
    };
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
