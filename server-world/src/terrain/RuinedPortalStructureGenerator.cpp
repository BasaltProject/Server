#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/TemplateBlockProcessors.hpp>
#include <basalt/server/world/terrain/RuinedPortalStructureGenerator.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using registry::BlockFace;
using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kSalt = 34222645;
constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kDefaultStructureIndex = 10;
constexpr float kGiantChance = 0.05F;
constexpr std::int32_t kMinYIndex = 15;
constexpr std::int32_t kMaxYDifference = 3;
constexpr std::int32_t kMaxDripLength = 8;
constexpr float kDripContinues = 0.5F;
constexpr float kMagmaChance = 0.07F;
constexpr float kLeavesChance = 0.5F;

/// A giant portal is 20 wide at most, so two chunks out covers every block it can reach.
constexpr std::int32_t kReachChunks = 3;
constexpr std::size_t kCacheCapacity = 64;

constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

/// parity: `Direction.Plane.HORIZONTAL`, whose order the direction draw depends on.
constexpr std::array<BlockFace, 4> kHorizontal{BlockFace::North, BlockFace::East, BlockFace::South, BlockFace::West};

constexpr std::array<float, 14> kNetherrackByDistance{1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F,
                                                      0.9F, 0.9F, 0.8F, 0.7F, 0.6F, 0.4F, 0.2F};

[[nodiscard]] std::int32_t blockState(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t magma() {
    static const std::int32_t id = blockState("magma_block");
    return id;
}

[[nodiscard]] std::int32_t netherrack() {
    static const std::int32_t id = blockState("netherrack");
    return id;
}

[[nodiscard]] std::int32_t jungleLeaves() {
    static const std::int32_t id = [] {
        const std::map<std::string, std::string> properties{{"persistent", "true"}};
        return registry::blocks::stateIdOf(wire::common::Identifier::vanilla("jungle_leaves"), properties)
            .value_or(blockState("jungle_leaves"));
    }();
    return id;
}

[[nodiscard]] std::string_view faceName(BlockFace face) {
    switch (face) {
    case BlockFace::North:
        return "north";
    case BlockFace::South:
        return "south";
    case BlockFace::West:
        return "west";
    case BlockFace::East:
        return "east";
    case BlockFace::Down:
        return "down";
    case BlockFace::Up:
    default:
        return "up";
    }
}

[[nodiscard]] std::optional<std::int32_t> vineTowards(BlockFace face) {
    const std::map<std::string, std::string> properties{{std::string{faceName(face)}, "true"}};
    return registry::blocks::stateIdOf(wire::common::Identifier::vanilla("vine"), properties);
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] const std::set<std::string, std::less<>>& featuresCannotReplace() {
    static const std::set<std::string, std::less<>> built{
        "bedrock", "spawner", "chest", "end_portal_frame", "reinforced_deepslate", "trial_spawner", "vault"};
    return built;
}

/// parity: `Biome.coldEnoughToSnow`, narrowed to the Overworld biomes that never thaw.
[[nodiscard]] bool isCold(std::string_view biome) {
    static const std::set<std::string, std::less<>> built{
        "snowy_plains", "ice_spikes", "snowy_taiga",  "snowy_beach",  "snowy_slopes",     "frozen_peaks",
        "jagged_peaks", "grove",      "frozen_river", "frozen_ocean", "deep_frozen_ocean"};
    const std::size_t colon = biome.find(':');
    return built.contains(colon == std::string_view::npos ? biome : biome.substr(colon + 1));
}

/// parity: the position of each variant among the 26 structures of `surface_structures`, in registry
/// order. They share a structure set but not an index, so they do not share a stream.
[[nodiscard]] std::int32_t structureIndexOf(std::string_view name) {
    static const std::map<std::string, std::int32_t, std::less<>> built{
        {"ruined_portal", 10},          {"ruined_portal_desert", 11}, {"ruined_portal_jungle", 12},
        {"ruined_portal_mountain", 13}, {"ruined_portal_nether", 14}, {"ruined_portal_ocean", 15},
        {"ruined_portal_swamp", 16}};
    const auto found = built.find(name);
    return found == built.end() ? kDefaultStructureIndex : found->second;
}

[[nodiscard]] const std::vector<std::string>& portalNames() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> all;
        for (std::int32_t index = 1; index <= 10; ++index) {
            all.push_back("ruined_portal/portal_" + std::to_string(index));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] const std::vector<std::string>& giantPortalNames() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> all;
        for (std::int32_t index = 1; index <= 3; ++index) {
            all.push_back("ruined_portal/giant_portal_" + std::to_string(index));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] bool sample(random::RandomSource& random, float limit) {
    if (limit == 0.0F) {
        return false;
    }
    if (limit == 1.0F) {
        return true;
    }
    return random.nextFloat() < limit;
}

[[nodiscard]] std::int32_t between(random::RandomSource& random, std::int32_t min, std::int32_t max) {
    return min + random.nextInt(max - min + 1);
}

[[nodiscard]] std::int32_t withinInterval(random::RandomSource& random, std::int32_t preferredMin, std::int32_t max) {
    return preferredMin < max ? between(random, preferredMin, max) : max;
}

[[nodiscard]] const RuinedPortalSetup* chooseSetup(random::RandomSource& random, const RuinedPortalVariant& variant) {
    if (variant.setups.size() == 1) {
        return &variant.setups.front();
    }
    float total = 0.0F;
    for (const RuinedPortalSetup& setup : variant.setups) {
        total += setup.weight;
    }
    float pick = random.nextFloat();
    for (const RuinedPortalSetup& setup : variant.setups) {
        pick -= setup.weight / total;
        if (pick < 0.0F) {
            return &setup;
        }
    }
    return nullptr;
}

} // namespace

RuinedPortalStructureGenerator::RuinedPortalStructureGenerator(std::vector<RuinedPortalVariant> variants,
                                                               TemplateOf templateOf, BiomeAt biomeAt,
                                                               HeightAt surfaceHeightAt, HeightAt oceanFloorAt,
                                                               IsSolidAt isSolidAt)
    : variants_(std::move(variants)), templateOf_(std::move(templateOf)), biomeAt_(std::move(biomeAt)),
      surfaceHeightAt_(std::move(surfaceHeightAt)), oceanFloorAt_(std::move(oceanFloorAt)),
      isSolidAt_(std::move(isSolidAt)), grid_{40, 15, kSalt} {}

std::int32_t RuinedPortalStructureGenerator::columnHeight(RuinedPortalPlacement placement, std::int32_t x,
                                                          std::int32_t z) const {
    return placement == RuinedPortalPlacement::OnOceanFloor ? oceanFloorAt_(x, z) : surfaceHeightAt_(x, z);
}

std::int32_t RuinedPortalStructureGenerator::frozenHeight(const Chunk& chunk, RuinedPortalPlacement placement,
                                                          std::int32_t x, std::int32_t z) const {
    if ((x >> 4) != chunk.chunkX() || (z >> 4) != chunk.chunkZ()) {
        return columnHeight(placement, x, z);
    }
    const HeightmapType type =
        placement == RuinedPortalPlacement::OnOceanFloor ? HeightmapType::OceanFloor : HeightmapType::WorldSurface;
    return chunk.worldgenHeightAt(x & 15, z & 15, type);
}

std::int32_t RuinedPortalStructureGenerator::suitableY(random::RandomSource& random, RuinedPortalPlacement placement,
                                                       bool airPocket, std::int32_t surfaceY, std::int32_t ySpan,
                                                       const StructureBoundingBox& box) const {
    const std::int32_t minY = kOverworldHeight.minY() + kMinYIndex;
    std::int32_t start = surfaceY;
    switch (placement) {
    case RuinedPortalPlacement::InNether:
        if (airPocket) {
            start = between(random, 32, 100);
        } else if (random.nextFloat() < 0.5F) {
            start = between(random, 27, 29);
        } else {
            start = between(random, 29, 100);
        }
        break;
    case RuinedPortalPlacement::InMountain:
        start = withinInterval(random, 70, surfaceY - ySpan);
        break;
    case RuinedPortalPlacement::Underground:
        start = withinInterval(random, minY, surfaceY - ySpan);
        break;
    case RuinedPortalPlacement::PartlyBuried:
        start = surfaceY - ySpan + between(random, 2, 8);
        break;
    default:
        break;
    }
    const std::array<std::pair<std::int32_t, std::int32_t>, 4> corners{
        std::pair{box.minX, box.minZ}, std::pair{box.maxX, box.minZ}, std::pair{box.minX, box.maxZ},
        std::pair{box.maxX, box.maxZ}};
    std::int32_t y = start;
    while (y > minY) {
        std::int32_t solidCorners = 0;
        for (const auto& [x, z] : corners) {
            if (isSolidAt_(x, y, z)) {
                ++solidCorners;
                if (solidCorners == 3) {
                    return y;
                }
            }
        }
        --y;
    }
    return y;
}

std::optional<RuinedPortalStructureGenerator::Portal>
RuinedPortalStructureGenerator::generationPoint(std::int32_t chunkX, std::int32_t chunkZ,
                                                const RuinedPortalVariant& variant, std::int64_t seed) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
    const RuinedPortalSetup* setup = chooseSetup(random, variant);
    if (setup == nullptr) {
        return std::nullopt;
    }
    const bool airPocket = sample(random, setup->airPocketProbability);
    const bool giant = random.nextFloat() < kGiantChance;
    const std::vector<std::string>& names = giant ? giantPortalNames() : portalNames();
    const structure::StructureTemplate* piece =
        templateOf_(names[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(names.size())))]);
    if (piece == nullptr) {
        return std::nullopt;
    }
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const StructureMirror mirror = random.nextFloat() < 0.5F ? StructureMirror::None : StructureMirror::FrontBack;
    const StructurePos pivot{piece->size().x / 2, 0, piece->size().z / 2};
    const StructurePos base{chunkX * 16, 0, chunkZ * 16};
    const StructureBoundingBox box = piece->getBoundingBox(base, rotation, pivot, mirror);
    const std::int32_t centreX = (box.minX + box.maxX) / 2;
    const std::int32_t centreZ = (box.minZ + box.maxZ) / 2;
    const std::int32_t surfaceY = columnHeight(setup->placement, centreX, centreZ) - 1;
    const std::int32_t ySpan = box.maxY - box.minY + 1;
    const std::int32_t y = suitableY(random, setup->placement, airPocket, surfaceY, ySpan, box);
    const StructurePos origin{base.x, y, base.z};
    const bool cold = setup->canBeCold && isCold(biomeAt_(origin.x, origin.y, origin.z));
    return Portal{.chunkX = chunkX,
                  .chunkZ = chunkZ,
                  .variantName = variant.name,
                  .setup = *setup,
                  .piece = piece,
                  .origin = origin,
                  .rotation = rotation,
                  .mirror = mirror,
                  .pivot = pivot,
                  .box = piece->getBoundingBox(origin, rotation, pivot, mirror),
                  .airPocket = airPocket,
                  .cold = cold};
}

std::optional<RuinedPortalStructureGenerator::Portal>
RuinedPortalStructureGenerator::assemble(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
    random::RandomSource pickRandom = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(pickRandom, seed, chunkX, chunkZ);
    std::vector<const RuinedPortalVariant*> remaining;
    remaining.reserve(variants_.size());
    for (const RuinedPortalVariant& variant : variants_) {
        remaining.push_back(&variant);
    }
    while (!remaining.empty()) {
        const std::size_t index =
            static_cast<std::size_t>(pickRandom.nextInt(static_cast<std::int32_t>(remaining.size())));
        const RuinedPortalVariant* chosen = remaining[index];
        const std::optional<Portal> portal = generationPoint(chunkX, chunkZ, *chosen, seed);
        // The tag keeps its namespace, and so does the biome name, so neither side is stripped.
        if (portal.has_value() &&
            chosen->biomes.contains(biomeAt_(portal->origin.x, portal->origin.y, portal->origin.z))) {
            return portal;
        }
        remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return std::nullopt;
}

std::shared_ptr<const RuinedPortalStructureGenerator::Portal>
RuinedPortalStructureGenerator::portalAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::int64_t key =
        (static_cast<std::int64_t>(chunkX) << 32) | (static_cast<std::int64_t>(chunkZ) & 0xFFFFFFFFLL);
    const std::scoped_lock lock{mutex_};
    const auto found = placed_.find(key);
    if (found != placed_.end()) {
        return found->second;
    }
    std::shared_ptr<const Portal> portal;
    if (grid_.isStructureChunk(seed, chunkX, chunkZ)) {
        if (std::optional<Portal> assembled = assemble(seed, chunkX, chunkZ); assembled.has_value()) {
            portal = std::make_shared<const Portal>(std::move(*assembled));
        }
    }
    if (placed_.size() >= kCacheCapacity) {
        placed_.clear();
    }
    return placed_.emplace(key, std::move(portal)).first->second;
}

void RuinedPortalStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const StructureBoundingBox clip{chunk.chunkX() * 16,      kOverworldHeight.minY(), chunk.chunkZ() * 16,
                                    chunk.chunkX() * 16 + 15, kOverworldHeight.maxY(), chunk.chunkZ() * 16 + 15};
    for (std::int32_t startX = chunk.chunkX() - kReachChunks; startX <= chunk.chunkX() + kReachChunks; ++startX) {
        for (std::int32_t startZ = chunk.chunkZ() - kReachChunks; startZ <= chunk.chunkZ() + kReachChunks; ++startZ) {
            const std::shared_ptr<const Portal> portal = portalAt(seed, startX, startZ);
            if (portal == nullptr || !portal->box.intersects(clip)) {
                continue;
            }
            place(*portal, chunk, clip, seed);
        }
    }
}

void RuinedPortalStructureGenerator::place(const Portal& portal, Chunk& chunk, const StructureBoundingBox& clip,
                                           std::int64_t seed) const {
    const RuinedPortalSetup& setup = portal.setup;
    std::vector<structure::TemplateBlockProcessor> processors;
    // parity: the block ignore processor comes first, and which one depends on the air pocket.
    // Without it the template's structure void is written as air, which buries the water a cave
    // portal is meant to sit in.
    processors.push_back(portal.airPocket ? structure::templateBlockProcessors::ignoreStructureBlock()
                                          : structure::templateBlockProcessors::ignoreStructureAndAir());
    processors.push_back(structure::templateBlockProcessors::age(setup.mossiness));
    processors.push_back(structure::templateBlockProcessors::protectFeatures());
    processors.push_back(structure::templateBlockProcessors::lavaSubmerged());
    if (setup.replaceWithBlackstone) {
        processors.push_back(structure::templateBlockProcessors::blackstoneReplace());
    }
    // parity: `RuinedPortalPiece.postProcess` takes the decoration random it is handed, not one of
    // its own, and passes it to `placeInWorld`, the netherrack, the drip columns and the vines. Each
    // of the seven variants owns its index within `surface_structures`, so the stream depends on
    // which was drawn.
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, structureIndexOf(portal.variantName), kSurfaceStructuresStep);
    portal.piece->placeInto(
        chunk, structure::TemplatePlacement{.position = portal.origin,
                                            .rotation = portal.rotation,
                                            .mirror = portal.mirror,
                                            .pivot = portal.pivot,
                                            .clipTo = clip,
                                            .blockProcessor = structure::templateBlockProcessors::chain(processors),
                                            .random = &random});
    spreadNetherrack(portal, chunk, clip, random);
    dripColumnsBelowPortal(portal, chunk, clip, random);
    if (setup.vines || setup.overgrown) {
        // parity: `BlockPos.betweenClosedStream` walks x fastest, then y, then z, and every non-air
        // block draws a direction whether or not a vine ends up on it.
        for (std::int32_t z = portal.box.minZ; z <= portal.box.maxZ; ++z) {
            for (std::int32_t y = portal.box.minY; y <= portal.box.maxY; ++y) {
                for (std::int32_t x = portal.box.minX; x <= portal.box.maxX; ++x) {
                    if (setup.vines) {
                        maybeAddVines(chunk, random, clip, x, y, z);
                    }
                    if (setup.overgrown) {
                        maybeAddLeavesAbove(chunk, random, clip, x, y, z);
                    }
                }
            }
        }
    }
}

void RuinedPortalStructureGenerator::maybeAddVines(Chunk& chunk, random::RandomSource& random,
                                                   const StructureBoundingBox& clip, std::int32_t x, std::int32_t y,
                                                   std::int32_t z) {
    const std::int32_t state = chunk.decorationBlockStateId(x, y, z);
    if (registry::blocks::isAir(state) || pathOf(state) == "vine") {
        return;
    }
    const BlockFace face =
        kHorizontal[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kHorizontal.size())))];
    const std::int32_t neighbourX = x + registry::stepX(face);
    const std::int32_t neighbourZ = z + registry::stepZ(face);
    const std::int32_t neighbour = chunk.decorationBlockStateId(neighbourX, y, neighbourZ);
    if (!registry::blocks::isAir(neighbour)) {
        return;
    }
    if (!registry::blockShapes::isCollisionFaceFull(state, face)) {
        return;
    }
    if (!clip.isInside(StructurePos{neighbourX, y, neighbourZ})) {
        return;
    }
    const std::optional<std::int32_t> vine = vineTowards(registry::opposite(face));
    if (!vine.has_value()) {
        return;
    }
    chunk.setDecorationBlockStateId(neighbourX, y, neighbourZ, *vine, neighbour);
}

void RuinedPortalStructureGenerator::maybeAddLeavesAbove(Chunk& chunk, random::RandomSource& random,
                                                         const StructureBoundingBox& clip, std::int32_t x,
                                                         std::int32_t y, std::int32_t z) {
    if (random.nextFloat() >= kLeavesChance) {
        return;
    }
    if (chunk.decorationBlockStateId(x, y, z) != netherrack()) {
        return;
    }
    const std::int32_t above = chunk.decorationBlockStateId(x, y + 1, z);
    if (!registry::blocks::isAir(above)) {
        return;
    }
    if (!clip.isInside(StructurePos{x, y + 1, z})) {
        return;
    }
    chunk.setDecorationBlockStateId(x, y + 1, z, jungleLeaves(), above);
}

void RuinedPortalStructureGenerator::dripColumnsBelowPortal(const Portal& portal, Chunk& chunk,
                                                            const StructureBoundingBox& clip,
                                                            random::RandomSource& random) {
    for (std::int32_t x = portal.box.minX + 1; x < portal.box.maxX; ++x) {
        for (std::int32_t z = portal.box.minZ + 1; z < portal.box.maxZ; ++z) {
            if (chunk.decorationBlockStateId(x, portal.box.minY, z) != netherrack()) {
                continue;
            }
            addDripColumn(chunk, random, portal, clip, x, portal.box.minY - 1, z);
        }
    }
}

void RuinedPortalStructureGenerator::spreadNetherrack(const Portal& portal, Chunk& chunk,
                                                      const StructureBoundingBox& clip,
                                                      random::RandomSource& random) const {
    const bool followsSurface = portal.setup.placement == RuinedPortalPlacement::OnLandSurface ||
                                portal.setup.placement == RuinedPortalPlacement::OnOceanFloor;
    const std::int32_t centreX = (portal.box.minX + portal.box.maxX) / 2;
    const std::int32_t centreZ = (portal.box.minZ + portal.box.maxZ) / 2;
    const std::int32_t averageWidth =
        ((portal.box.maxX - portal.box.minX + 1) + (portal.box.maxZ - portal.box.minZ + 1)) / 2;
    const std::int32_t adjustment = random.nextInt(std::max(1, 8 - averageWidth / 2));
    const std::int32_t reach = static_cast<std::int32_t>(kNetherrackByDistance.size());
    for (std::int32_t x = centreX - reach; x <= centreX + reach; ++x) {
        for (std::int32_t z = centreZ - reach; z <= centreZ + reach; ++z) {
            const std::int32_t distance = std::max(0, std::abs(x - centreX) + std::abs(z - centreZ) + adjustment);
            if (distance >= reach) {
                continue;
            }
            if (random.nextDouble() >= static_cast<double>(kNetherrackByDistance[static_cast<std::size_t>(distance)])) {
                continue;
            }
            const std::int32_t surfaceY = frozenHeight(chunk, portal.setup.placement, x, z) - 1;
            const std::int32_t y = followsSurface ? surfaceY : std::min(portal.box.minY, surfaceY);
            if (std::abs(y - portal.box.minY) > kMaxYDifference) {
                continue;
            }
            if (!clip.isInside(StructurePos{x, y, z})) {
                continue;
            }
            const std::int32_t existing = chunk.decorationBlockStateId(x, y, z);
            if (registry::blocks::isAir(existing)) {
                continue;
            }
            const std::string path = pathOf(existing);
            if (path.empty() || path == "obsidian" || featuresCannotReplace().contains(path)) {
                continue;
            }
            if (portal.setup.placement != RuinedPortalPlacement::InNether && path == "lava") {
                continue;
            }
            placeNetherrackOrMagma(chunk, random, portal, x, y, z);
            if (portal.setup.overgrown) {
                maybeAddLeavesAbove(chunk, random, clip, x, y, z);
            }
            addDripColumn(chunk, random, portal, clip, x, y - 1, z);
        }
    }
}

void RuinedPortalStructureGenerator::addDripColumn(Chunk& chunk, random::RandomSource& random, const Portal& portal,
                                                   const StructureBoundingBox& clip, std::int32_t x,
                                                   std::int32_t startY, std::int32_t z) {
    std::int32_t y = startY;
    if (clip.isInside(StructurePos{x, y, z})) {
        placeNetherrackOrMagma(chunk, random, portal, x, y, z);
    }
    std::int32_t remaining = kMaxDripLength;
    while (remaining > 0 && random.nextFloat() < kDripContinues) {
        --y;
        --remaining;
        if (clip.isInside(StructurePos{x, y, z})) {
            placeNetherrackOrMagma(chunk, random, portal, x, y, z);
        }
    }
}

void RuinedPortalStructureGenerator::placeNetherrackOrMagma(Chunk& chunk, random::RandomSource& random,
                                                            const Portal& portal, std::int32_t x, std::int32_t y,
                                                            std::int32_t z) {
    const std::int32_t state = (!portal.cold && random.nextFloat() < kMagmaChance) ? magma() : netherrack();
    chunk.setDecorationBlockStateId(x, y, z, state, chunk.decorationBlockStateId(x, y, z));
}

} // namespace basalt::server::world::terrain
