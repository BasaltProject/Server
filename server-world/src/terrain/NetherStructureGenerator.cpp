#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/NetherFortressPlacement.hpp>
#include <basalt/server/world/terrain/NetherStructureGenerator.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>
#include <basalt/server/world/terrain/StructureReferenceOrder.hpp>
#include <basalt/server/world/terrain/carver/Carvers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::PlacedStructurePiece;
using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kNetherSeaLevel = 32;
constexpr std::size_t kSamplerCacheCapacity = 64;
constexpr std::int32_t kFossilMinY = 32;
constexpr std::int32_t kFossilMaxY = 125;
constexpr std::int32_t kFossilSearchCells = 2;
constexpr std::int32_t kBastionStartY = 33;
constexpr std::int32_t kFortressStartY = 64;
constexpr std::int32_t kFortressWeight = 2;
constexpr std::int32_t kNetherComplexTotalWeight = 5;
constexpr std::string_view kFortressName = "fortress";
constexpr std::int32_t kFortressStructureIndex = 1;
constexpr std::int32_t kBastionStructureIndex = 18;
constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kUndergroundDecorationStep = 7;
constexpr std::int32_t kStructureReferenceRange = 8;
constexpr StructurePos kZeroPos{0, 0, 0};

constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

[[nodiscard]] const structure::StructurePlacementGrid& fossilGrid() {
    static const structure::StructurePlacementGrid built{2, 1, 14357921};
    return built;
}

[[nodiscard]] const structure::StructurePlacementGrid& complexGrid() {
    static const structure::StructurePlacementGrid built{27, 4, 30084232};
    return built;
}

[[nodiscard]] const std::vector<std::string>& fossilTemplates() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> all;
        for (std::int32_t index = 1; index <= 14; ++index) {
            all.push_back("minecraft:nether_fossils/fossil_" + std::to_string(index));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] const std::set<std::string, std::less<>>& bastionBiomes() {
    static const std::set<std::string, std::less<>> built{"crimson_forest", "nether_wastes", "soul_sand_valley",
                                                          "warped_forest"};
    return built;
}

[[nodiscard]] const std::set<std::string, std::less<>>& netherBiomes() {
    static const std::set<std::string, std::less<>> built{"crimson_forest", "nether_wastes", "soul_sand_valley",
                                                          "warped_forest", "basalt_deltas"};
    return built;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] std::int32_t netherrack() {
    static const std::int32_t id = [] {
        const registry::BlockState* found = registry::blocks::named("netherrack");
        return found == nullptr ? registry::blocks::air().stateId : found->stateId;
    }();
    return id;
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

template <typename Boxed>
[[nodiscard]] StructureBoundingBox unionOf(const std::vector<Boxed>& pieces) {
    StructureBoundingBox result = pieces.front().boundingBox;
    for (std::size_t index = 1; index < pieces.size(); ++index) {
        const StructureBoundingBox& box = pieces[index].boundingBox;
        result = StructureBoundingBox{std::min(result.minX, box.minX), std::min(result.minY, box.minY),
                                      std::min(result.minZ, box.minZ), std::max(result.maxX, box.maxX),
                                      std::max(result.maxY, box.maxY), std::max(result.maxZ, box.maxZ)};
    }
    return result;
}

[[nodiscard]] StructureBoundingBox chunkClip(const Chunk& chunk) {
    return StructureBoundingBox{chunk.chunkX() * 16,      chunk.verticalBounds().minY(), chunk.chunkZ() * 16,
                                chunk.chunkX() * 16 + 15, chunk.verticalBounds().maxY(), chunk.chunkZ() * 16 + 15};
}

[[nodiscard]] StructureBoundingBox chunkColumnClip(std::int32_t chunkX, std::int32_t chunkZ) {
    return StructureBoundingBox{chunkX * 16,      std::numeric_limits<std::int32_t>::min(), chunkZ * 16,
                                chunkX * 16 + 15, std::numeric_limits<std::int32_t>::max(), chunkZ * 16 + 15};
}

/// The references a chunk keeps, in the order vanilla's own map hands them back.
template <typename Reference>
[[nodiscard]] std::vector<Reference> inReferenceOrder(std::vector<Reference> references) {
    std::vector<std::pair<std::int32_t, std::int32_t>> insertionOrder;
    insertionOrder.reserve(references.size());
    for (const Reference& reference : references) {
        insertionOrder.emplace_back(reference.chunkX, reference.chunkZ);
    }
    std::sort(insertionOrder.begin(), insertionOrder.end());
    std::vector<Reference> ordered;
    ordered.reserve(references.size());
    for (const auto& [orderedX, orderedZ] : vanillaStructureReferenceChunkOrder(insertionOrder)) {
        for (const Reference& reference : references) {
            if (reference.chunkX == orderedX && reference.chunkZ == orderedZ) {
                ordered.push_back(reference);
                break;
            }
        }
    }
    return ordered;
}

struct BastionReference {
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    const std::vector<PlacedStructurePiece>* pieces = nullptr;
};

struct FortressReference {
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    const std::vector<netherFortressPieces::Piece>* pieces = nullptr;
};

/// A biome name that outlives the lookup that produced it, as the carvers keep references.
[[nodiscard]] const std::string& internedBiome(std::string_view name) {
    static std::mutex mutex;
    static std::set<std::string, std::less<>> names;
    const std::lock_guard<std::mutex> guard{mutex};
    if (const auto found = names.find(name); found != names.end()) {
        return *found;
    }
    return *names.emplace(name).first;
}

} // namespace

NetherStructureGenerator::NetherStructureGenerator(NoiseRouter router, const NetherBiomeSource& biomeSource,
                                                   structure::TemplateLoader templateLoader, PoolLookup poolLookup)
    : router_(std::move(router)), biomeSource_(&biomeSource), templateLoader_(std::move(templateLoader)),
      poolLookup_(std::move(poolLookup)) {
    ruinedPortals_ = std::make_unique<NetherRuinedPortalGenerator>(
        biomeSource, templateLoader_,
        [this](std::int32_t x, std::int32_t y, std::int32_t z) { return baseStateAt(x, y, z); },
        [this](Chunk& chunk, std::int64_t seed) { placeBastions(chunk, seed); },
        [this](Chunk& chunk) {
            carver::carveShape(
                chunk, router_,
                [this](std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) -> const std::string& {
                    return internedBiome(biomeSource_->biomeAtQuart(quartX, quartY, quartZ));
                });
        });
}

void NetherStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    placeBastions(chunk, seed);
    ruinedPortals_->generateInto(chunk, seed);
    placeNetherFossils(chunk, seed);
    placeFortresses(chunk, seed);
}

DensityChunkSampler& NetherStructureGenerator::samplerAt(std::int32_t x, std::int32_t z) {
    const std::pair<std::int32_t, std::int32_t> key{x >> 4, z >> 4};
    const auto found = samplers_.find(key);
    if (found != samplers_.end()) {
        return *found->second;
    }
    auto created = std::make_unique<DensityChunkSampler>(router_, key.first, key.second);
    // One sampler per chunk with no ceiling is a leak in a server that keeps exploring: each holds its
    // chunk's whole density volume. Clearing is safe here, and only here, because the only caller
    // reads the sampler with `mutex_` still held, so no reference outlives this map.
    if (samplers_.size() >= kSamplerCacheCapacity) {
        samplers_.clear();
    }
    return *samplers_.emplace(key, std::move(created)).first->second;
}

std::int32_t NetherStructureGenerator::baseStateAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    // Every chunk generation worker shares this generator, so the sampler map needs the lock, and a
    // sampler must not be cleared out from under a reader.
    double density = 0.0;
    {
        const std::scoped_lock lock{mutex_};
        density = samplerAt(x, z).sample(x & 15, y, z & 15);
    }
    if (density > 0.0) {
        return netherrack();
    }
    return y < kNetherSeaLevel ? registry::blocks::lava().stateId : registry::blocks::air().stateId;
}

bool NetherStructureGenerator::selectsBastion(std::int64_t seed, std::int32_t candidateChunkX,
                                              std::int32_t candidateChunkZ) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    return random.nextInt(kNetherComplexTotalWeight) >= kFortressWeight;
}

bool NetherStructureGenerator::selectsFortress(std::int64_t seed, std::int32_t candidateChunkX,
                                               std::int32_t candidateChunkZ) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    return random.nextInt(kNetherComplexTotalWeight) < kFortressWeight;
}

const std::vector<PlacedStructurePiece>& NetherStructureGenerator::assembledBastion(std::int64_t seed,
                                                                                    std::int32_t candidateChunkX,
                                                                                    std::int32_t candidateChunkZ) {
    const CacheKey key{seed, candidateChunkX, candidateChunkZ};
    const auto found = bastions_.find(key);
    if (found != bastions_.end()) {
        return found->second;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    const structure::JigsawWorld world{
        .poolLookup = poolLookup_,
        .templateLoader =
            [this](std::string_view name) {
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                    wire::common::Identifier::parse(name);
                return parsed.has_value() ? templateLoader_(*parsed) : nullptr;
            },
        .heightAt = [](std::int32_t, std::int32_t) { return 0; }};
    std::vector<PlacedStructurePiece> assembled = structure::assembleJigsaw(
        world,
        structure::JigsawRequest{.startPool = "minecraft:bastion/starts",
                                 .startPosition =
                                     StructurePos{candidateChunkX * 16, kBastionStartY, candidateChunkZ * 16},
                                 .maxDepth = 6,
                                 .projectStartToHeightmap = false,
                                 .maxDistanceFromCenter = 80,
                                 .maxVerticalDistanceFromCenter = 80},
        random);
    bool valid = !assembled.empty();
    if (valid) {
        const StructureBoundingBox& startBox = assembled.front().boundingBox;
        const std::int32_t centreX = (startBox.minX + startBox.maxX) / 2;
        const std::int32_t centreZ = (startBox.minZ + startBox.maxZ) / 2;
        valid = bastionBiomes().contains(biomePath(biomeSource_->biomeAt(centreX, kBastionStartY, centreZ)));
    }
    if (!valid) {
        assembled.clear();
    }
    return bastions_.emplace(key, std::move(assembled)).first->second;
}

const std::vector<netherFortressPieces::Piece>&
NetherStructureGenerator::assembledFortress(std::int64_t seed, std::int32_t candidateChunkX,
                                            std::int32_t candidateChunkZ) {
    const CacheKey key{seed, candidateChunkX, candidateChunkZ};
    const auto found = fortresses_.find(key);
    if (found != fortresses_.end()) {
        return found->second;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    std::vector<netherFortressPieces::Piece> assembled;
    if (netherBiomes().contains(
            biomePath(biomeSource_->biomeAt(candidateChunkX * 16, kFortressStartY, candidateChunkZ * 16)))) {
        assembled = netherFortressPieces::assemble(random, candidateChunkX, candidateChunkZ);
    }
    return fortresses_.emplace(key, std::move(assembled)).first->second;
}

void NetherStructureGenerator::placeBastions(Chunk& chunk, std::int64_t seed) {
    const std::int32_t spacing = complexGrid().spacing();
    const std::int32_t cellX = floorDiv(chunk.chunkX(), spacing);
    const std::int32_t cellZ = floorDiv(chunk.chunkZ(), spacing);
    const StructureBoundingBox clip = chunkClip(chunk);
    random::RandomSource placementRandom = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(placementRandom, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(placementRandom, decorationSeed, kBastionStructureIndex, kSurfaceStructuresStep);
    std::vector<BastionReference> references;
    for (std::int32_t cellOffsetX = -1; cellOffsetX <= 1; ++cellOffsetX) {
        for (std::int32_t cellOffsetZ = -1; cellOffsetZ <= 1; ++cellOffsetZ) {
            const auto [candidateX, candidateZ] =
                complexGrid().candidateChunk(seed, (cellX + cellOffsetX) * spacing, (cellZ + cellOffsetZ) * spacing);
            if (candidateX < chunk.chunkX() - kStructureReferenceRange ||
                candidateX > chunk.chunkX() + kStructureReferenceRange) {
                continue;
            }
            if (candidateZ < chunk.chunkZ() - kStructureReferenceRange ||
                candidateZ > chunk.chunkZ() + kStructureReferenceRange) {
                continue;
            }
            if (!selectsBastion(seed, candidateX, candidateZ)) {
                continue;
            }
            const std::vector<PlacedStructurePiece>& pieces = assembledBastion(seed, candidateX, candidateZ);
            if (pieces.empty() || !unionOf(pieces).intersects(clip)) {
                continue;
            }
            references.push_back(BastionReference{candidateX, candidateZ, &pieces});
        }
    }
    for (const BastionReference& reference : inReferenceOrder(std::move(references))) {
        for (const PlacedStructurePiece& piece : *reference.pieces) {
            if (!piece.boundingBox.intersects(clip)) {
                continue;
            }
            piece.element.place(
                [this](std::string_view name) {
                    const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                        wire::common::Identifier::parse(name);
                    return parsed.has_value() ? templateLoader_(*parsed) : nullptr;
                },
                chunk, piece.position, piece.rotation, clip, placementRandom);
        }
    }
}

void NetherStructureGenerator::placeFortresses(Chunk& chunk, std::int64_t seed) {
    const std::int32_t spacing = complexGrid().spacing();
    const std::int32_t cellX = floorDiv(chunk.chunkX(), spacing);
    const std::int32_t cellZ = floorDiv(chunk.chunkZ(), spacing);
    const StructureBoundingBox clip = chunkClip(chunk);
    random::RandomSource placementRandom = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(placementRandom, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(placementRandom, decorationSeed, kFortressStructureIndex, kUndergroundDecorationStep);
    std::vector<FortressReference> references;
    for (std::int32_t cellOffsetX = -1; cellOffsetX <= 1; ++cellOffsetX) {
        for (std::int32_t cellOffsetZ = -1; cellOffsetZ <= 1; ++cellOffsetZ) {
            const auto [candidateX, candidateZ] =
                complexGrid().candidateChunk(seed, (cellX + cellOffsetX) * spacing, (cellZ + cellOffsetZ) * spacing);
            if (candidateX < chunk.chunkX() - kStructureReferenceRange ||
                candidateX > chunk.chunkX() + kStructureReferenceRange) {
                continue;
            }
            if (candidateZ < chunk.chunkZ() - kStructureReferenceRange ||
                candidateZ > chunk.chunkZ() + kStructureReferenceRange) {
                continue;
            }
            if (!selectsFortress(seed, candidateX, candidateZ)) {
                continue;
            }
            const std::vector<netherFortressPieces::Piece>& pieces = assembledFortress(seed, candidateX, candidateZ);
            if (pieces.empty() || !unionOf(pieces).intersects(clip)) {
                continue;
            }
            references.push_back(FortressReference{candidateX, candidateZ, &pieces});
        }
    }
    // Records the slice of a fortress that reaches this chunk. The monster override tests piece
    // bounds, so only the pieces this chunk holds are kept; the full bounds serve the separate
    // nether-brick gate, which tests the fortress as a whole.
    for (const FortressReference& reference : references) {
        std::vector<StructureBoundingBox> here;
        for (const netherFortressPieces::Piece& piece : *reference.pieces) {
            if (piece.boundingBox.intersects(clip)) {
                here.push_back(piece.boundingBox);
            }
        }
        if (here.empty()) {
            continue;
        }
        chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kFortressName},
                                                               .chunkX = reference.chunkX,
                                                               .chunkZ = reference.chunkZ,
                                                               .fullBounds = unionOf(*reference.pieces),
                                                               .pieces = std::move(here)});
    }
    for (const FortressReference& reference : inReferenceOrder(std::move(references))) {
        for (const netherFortressPieces::Piece& piece : *reference.pieces) {
            if (piece.boundingBox.intersects(clip)) {
                netherFortressPieces::place(piece, chunk, clip, placementRandom);
            }
        }
    }
}

std::optional<NetherStructureGenerator::FossilStart>
NetherStructureGenerator::netherFossilStart(std::int32_t candidateChunkX, std::int32_t candidateChunkZ,
                                            std::int64_t seed) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    const std::int32_t x = candidateChunkX * 16 + random.nextInt(16);
    const std::int32_t z = candidateChunkZ * 16 + random.nextInt(16);
    std::int32_t y = kFossilMinY + random.nextInt(kFossilMaxY - kFossilMinY + 1);
    std::int32_t current = baseStateAt(x, y, z);
    while (y > kNetherSeaLevel) {
        const std::int32_t below = baseStateAt(x, --y, z);
        if (registry::blocks::isAir(current) && below == netherrack()) {
            break;
        }
        current = below;
    }
    if (y <= kNetherSeaLevel) {
        return std::nullopt;
    }
    if (biomePath(biomeSource_->biomeAt(x, y, z)) != "soul_sand_valley") {
        return std::nullopt;
    }
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const std::string templateName = fossilTemplates()[static_cast<std::size_t>(
        random.nextInt(static_cast<std::int32_t>(fossilTemplates().size())))];
    return FossilStart{.position = StructurePos{x, y, z}, .rotation = rotation, .templateName = templateName};
}

std::vector<NetherStructureGenerator::FossilReference>
NetherStructureGenerator::referencedNetherFossils(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed) {
    const std::int32_t spacing = fossilGrid().spacing();
    const std::int32_t cellX = floorDiv(chunkX, spacing);
    const std::int32_t cellZ = floorDiv(chunkZ, spacing);
    const StructureBoundingBox clip = chunkColumnClip(chunkX, chunkZ);
    std::vector<FossilReference> references;
    for (std::int32_t cellOffsetX = -kFossilSearchCells; cellOffsetX <= kFossilSearchCells; ++cellOffsetX) {
        for (std::int32_t cellOffsetZ = -kFossilSearchCells; cellOffsetZ <= kFossilSearchCells; ++cellOffsetZ) {
            const auto [candidateX, candidateZ] =
                fossilGrid().candidateChunk(seed, (cellX + cellOffsetX) * spacing, (cellZ + cellOffsetZ) * spacing);
            const std::optional<FossilStart> start = netherFossilStart(candidateX, candidateZ, seed);
            if (!start.has_value()) {
                continue;
            }
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                wire::common::Identifier::parse(start->templateName);
            const structure::StructureTemplate* piece = parsed.has_value() ? templateLoader_(*parsed) : nullptr;
            if (piece == nullptr) {
                continue;
            }
            const StructureBoundingBox boundingBox =
                piece->getBoundingBox(start->position, start->rotation, kZeroPos, StructureMirror::None);
            if (!boundingBox.intersects(clip)) {
                continue;
            }
            references.push_back(FossilReference{candidateX, candidateZ, *start, piece, boundingBox});
        }
    }
    return inReferenceOrder(std::move(references));
}

void NetherStructureGenerator::placeNetherFossils(Chunk& chunk, std::int64_t seed) {
    const StructureBoundingBox clip = chunkClip(chunk);
    for (const FossilReference& reference : referencedNetherFossils(chunk.chunkX(), chunk.chunkZ(), seed)) {
        reference.piece->placeInto(chunk, structure::TemplatePlacement{.position = reference.start.position,
                                                                       .rotation = reference.start.rotation,
                                                                       .mirror = StructureMirror::None,
                                                                       .pivot = kZeroPos,
                                                                       .clipTo = clip,
                                                                       .ignoreStructureBlocks = true,
                                                                       .ignoreAir = true});
        placeDriedGhast(chunk, seed, reference.boundingBox, clip);
    }
}

void NetherStructureGenerator::placeDriedGhast(Chunk& chunk, std::int64_t seed, const StructureBoundingBox& fossilBox,
                                               const StructureBoundingBox& clip) const {
    const StructurePos centre{fossilBox.minX + (fossilBox.maxX - fossilBox.minX + 1) / 2,
                              fossilBox.minY + (fossilBox.maxY - fossilBox.minY + 1) / 2,
                              fossilBox.minZ + (fossilBox.maxZ - fossilBox.minZ + 1) / 2};
    random::RandomSource random = random::RandomSource::legacy(seed).forkPositional().at(centre.x, centre.y, centre.z);
    if (random.nextFloat() >= 0.5F) {
        return;
    }
    const StructurePos position{fossilBox.minX + random.nextInt(fossilBox.maxX - fossilBox.minX + 1), fossilBox.minY,
                                fossilBox.minZ + random.nextInt(fossilBox.maxZ - fossilBox.minZ + 1)};
    if (!clip.isInside(position)) {
        return;
    }
    if (!registry::blocks::isAir(chunk.blockStateId(position.x & 15, position.y, position.z & 15))) {
        return;
    }
    static_cast<void>(random.nextInt(static_cast<std::int32_t>(kRotations.size())));
    static const std::int32_t driedGhast = [] {
        const registry::BlockState* found = registry::blocks::named("dried_ghast");
        return found == nullptr ? registry::blocks::air().stateId : found->stateId;
    }();
    chunk.setBlockStateId(position.x & 15, position.y, position.z & 15, driedGhast);
}

densityfunction::Sampler NetherStructureGenerator::beardifierForChunk(std::int32_t chunkX, std::int32_t chunkZ,
                                                                      std::int64_t seed) {
    StructureBeard beard;
    for (const FossilReference& reference : referencedNetherFossils(chunkX, chunkZ, seed)) {
        beard.pieces.push_back(BeardifierRigid{.box = reference.boundingBox});
    }
    return structureBeardifier(std::move(beard));
}

} // namespace basalt::server::world::terrain
