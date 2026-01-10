#include <basalt/server/random/PositionalHashing.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/NetherRuinedPortalGenerator.hpp>
#include <basalt/server/world/terrain/carver/NetherCaveCarver.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr std::int32_t kNetherGenerationDepth = 128;
constexpr std::int32_t kMinProjectedY = 15;
constexpr std::int32_t kSpreadRadius = 14;
constexpr float kAirPocketProbability = 0.5F;
constexpr float kGiantPortalProbability = 0.05F;
constexpr float kMagmaProbability = 0.07F;
constexpr std::int32_t kRuinedPortalStructureIndex = 14;
constexpr std::int32_t kSurfaceStructuresStep = 4;

constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

constexpr std::array<float, 14> kNetherrackByDistance{1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F,
                                                      0.9F, 0.9F, 0.8F, 0.7F, 0.6F, 0.4F, 0.2F};

[[nodiscard]] const structure::StructurePlacementGrid& portalGrid() {
    static const structure::StructurePlacementGrid built{40, 15, 34222645};
    return built;
}

[[nodiscard]] std::int32_t plainState(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t netherrack() {
    static const std::int32_t id = plainState("netherrack");
    return id;
}

[[nodiscard]] std::int32_t magma() {
    static const std::int32_t id = plainState("magma_block");
    return id;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] const std::set<std::string, std::less<>>& protectedBlocks() {
    static const std::set<std::string, std::less<>> built{
        "bedrock", "spawner", "chest", "end_portal_frame", "reinforced_deepslate", "trial_spawner", "vault"};
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

[[nodiscard]] const std::vector<std::string>& portals() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> all;
        for (std::int32_t index = 1; index <= 10; ++index) {
            all.push_back("minecraft:ruined_portal/portal_" + std::to_string(index));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] const std::vector<std::string>& giantPortals() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> all;
        for (std::int32_t index = 1; index <= 3; ++index) {
            all.push_back("minecraft:ruined_portal/giant_portal_" + std::to_string(index));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] StructurePos centreOf(const StructureBoundingBox& box) {
    return StructurePos{box.minX + (box.maxX - box.minX + 1) / 2, box.minY + (box.maxY - box.minY + 1) / 2,
                        box.minZ + (box.maxZ - box.minZ + 1) / 2};
}

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

/// A block as the portal's processors see it: a path plus whatever properties the template carries,
/// which is the only shape the rule, ageing and blackstone passes can rewrite.
struct PortalState {
    std::string path;
    std::map<std::string, std::string> properties;

    [[nodiscard]] bool isAir() const { return path == "air" || path == "cave_air" || path == "void_air"; }

    [[nodiscard]] std::int32_t stateId() const {
        const wire::common::Identifier identifier = wire::common::Identifier::vanilla(path);
        const std::optional<std::int32_t> found = registry::blocks::stateIdOf(identifier, properties);
        if (found.has_value()) {
            return *found;
        }
        return plainState(path);
    }

    [[nodiscard]] bool hasFullCollisionShape() const {
        static const std::set<std::string, std::less<>> nonFull{"lava", "water", "iron_chain", "iron_bars", "chest"};
        if (isAir() || nonFull.contains(path)) {
            return false;
        }
        const auto type = properties.find("type");
        if (path.ends_with("_slab") && (type == properties.end() || type->second != "double")) {
            return false;
        }
        return !path.ends_with("_stairs") && !path.ends_with("_wall");
    }

    [[nodiscard]] PortalState transform(StructureMirror mirror, StructureRotation rotation) const {
        const auto facing = properties.find("facing");
        if (facing == properties.end()) {
            return *this;
        }
        const std::optional<structure::StructureDirection> direction = structure::directionFromName(facing->second);
        if (!direction.has_value()) {
            return *this;
        }
        PortalState turned = *this;
        turned.properties["facing"] = std::string{
            structure::serializedName(structure::rotated(structure::mirrored(*direction, mirror), rotation))};
        return turned;
    }

    [[nodiscard]] static PortalState of(std::string_view path) { return PortalState{std::string{path}, {}}; }

    [[nodiscard]] static PortalState bottomSlab(std::string_view path) {
        return PortalState{std::string{path}, {{"type", "bottom"}, {"waterlogged", "false"}}};
    }

    [[nodiscard]] static PortalState from(std::int32_t stateId,
                                          const std::map<std::string, std::string>& templateProperties) {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        if (block == nullptr) {
            return of("air");
        }
        return PortalState{block->path(), templateProperties.empty()
                                              ? registry::blockStateDefinitions::propertiesOf(stateId)
                                              : templateProperties};
    }
};

[[nodiscard]] random::RandomSource positionalRandom(StructurePos position) {
    return random::RandomSource::worldgen(
        random::RandomSource::legacy(random::positionalSeed(position.x, position.y, position.z)));
}

[[nodiscard]] PortalState randomStairs(random::RandomSource& random, std::string_view path) {
    static constexpr std::array<std::string_view, 4> kFacings{"north", "east", "south", "west"};
    static constexpr std::array<std::string_view, 2> kHalves{"top", "bottom"};
    return PortalState{std::string{path},
                       {{"facing", std::string{kFacings[static_cast<std::size_t>(random.nextInt(4))]}},
                        {"half", std::string{kHalves[static_cast<std::size_t>(random.nextInt(2))]}},
                        {"shape", "straight"},
                        {"waterlogged", "false"}}};
}

[[nodiscard]] PortalState applyRuleProcessor(const PortalState& input, StructurePos position) {
    random::RandomSource random = positionalRandom(position);
    if (input.path == "gold_block") {
        return random.nextFloat() < 0.3F ? PortalState::of("air") : input;
    }
    if (input.path == "lava") {
        return random.nextFloat() < 0.2F ? PortalState::of("magma_block") : input;
    }
    if (input.path == "netherrack") {
        return random.nextFloat() < kMagmaProbability ? PortalState::of("magma_block") : input;
    }
    return input;
}

[[nodiscard]] PortalState applyBlockAgeProcessor(const PortalState& input, StructurePos position) {
    static const std::set<std::string, std::less<>> stairBlocks{"cobblestone_stairs", "mossy_cobblestone_stairs",
                                                                "stone_stairs", "stone_brick_stairs",
                                                                "mossy_stone_brick_stairs"};
    static const std::set<std::string, std::less<>> slabBlocks{"cobblestone_slab",  "mossy_cobblestone_slab",
                                                               "smooth_stone_slab", "stone_slab",
                                                               "stone_brick_slab",  "mossy_stone_brick_slab"};
    static const std::set<std::string, std::less<>> wallBlocks{"stone_brick_wall", "mossy_stone_brick_wall",
                                                               "cobblestone_wall", "mossy_cobblestone_wall"};
    random::RandomSource random = positionalRandom(position);
    if (input.path == "stone_bricks" || input.path == "stone" || input.path == "chiseled_stone_bricks") {
        if (random.nextFloat() >= 0.5F) {
            return input;
        }
        const PortalState nonMossyStairs = randomStairs(random, "stone_brick_stairs");
        static_cast<void>(randomStairs(random, "mossy_stone_brick_stairs"));
        static_cast<void>(random.nextFloat());
        return random.nextInt(2) == 0 ? PortalState::of("cracked_stone_bricks") : nonMossyStairs;
    }
    if (stairBlocks.contains(input.path)) {
        if (random.nextFloat() >= 0.5F) {
            return input;
        }
        static_cast<void>(random.nextFloat());
        return random.nextInt(2) == 0 ? PortalState::bottomSlab("stone_slab")
                                      : PortalState::bottomSlab("stone_brick_slab");
    }
    if (slabBlocks.contains(input.path) || wallBlocks.contains(input.path)) {
        static_cast<void>(random.nextFloat());
        return input;
    }
    if (input.path == "obsidian") {
        return random.nextFloat() < 0.15F ? PortalState::of("crying_obsidian") : input;
    }
    return input;
}

[[nodiscard]] PortalState applyBlackstoneReplacement(const PortalState& input) {
    static const std::map<std::string, std::string, std::less<>> replacements{
        {"cobblestone", "blackstone"},
        {"mossy_cobblestone", "blackstone"},
        {"stone", "polished_blackstone"},
        {"stone_bricks", "polished_blackstone_bricks"},
        {"mossy_stone_bricks", "polished_blackstone_bricks"},
        {"cobblestone_stairs", "blackstone_stairs"},
        {"mossy_cobblestone_stairs", "blackstone_stairs"},
        {"stone_stairs", "polished_blackstone_stairs"},
        {"stone_brick_stairs", "polished_blackstone_brick_stairs"},
        {"mossy_stone_brick_stairs", "polished_blackstone_brick_stairs"},
        {"cobblestone_slab", "blackstone_slab"},
        {"mossy_cobblestone_slab", "blackstone_slab"},
        {"smooth_stone_slab", "polished_blackstone_slab"},
        {"stone_slab", "polished_blackstone_slab"},
        {"stone_brick_slab", "polished_blackstone_brick_slab"},
        {"mossy_stone_brick_slab", "polished_blackstone_brick_slab"},
        {"stone_brick_wall", "polished_blackstone_brick_wall"},
        {"mossy_stone_brick_wall", "polished_blackstone_brick_wall"},
        {"cobblestone_wall", "blackstone_wall"},
        {"mossy_cobblestone_wall", "blackstone_wall"},
        {"chiseled_stone_bricks", "chiseled_polished_blackstone"},
        {"cracked_stone_bricks", "cracked_polished_blackstone_bricks"},
        {"iron_bars", "iron_chain"}};
    const auto found = replacements.find(input.path);
    if (found == replacements.end()) {
        return input;
    }
    std::map<std::string, std::string> properties;
    const auto facing = input.properties.find("facing");
    const auto half = input.properties.find("half");
    const auto type = input.properties.find("type");
    if (facing != input.properties.end() && half != input.properties.end()) {
        properties = {
            {"facing", facing->second}, {"half", half->second}, {"shape", "straight"}, {"waterlogged", "false"}};
    } else if (type != input.properties.end()) {
        properties = {{"type", type->second}, {"waterlogged", "false"}};
    }
    return PortalState{found->second, std::move(properties)};
}

[[nodiscard]] bool canReplaceWithNetherrack(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    if (path.empty()) {
        return false;
    }
    return path != "air" && path != "obsidian" && !protectedBlocks().contains(path);
}

} // namespace

/// The portal's start, kept whole because every later step reads it.
struct NetherRuinedPortalGenerator::PortalStart {
    std::string templateName;
    const structure::StructureTemplate* piece = nullptr;
    StructurePos position;
    StructureRotation rotation = StructureRotation::None;
    StructureMirror mirror = StructureMirror::None;
    StructurePos pivot;
    StructureBoundingBox boundingBox;
    bool airPocket = false;

    [[nodiscard]] bool effectIntersects(const StructureBoundingBox& clip) const {
        const StructurePos centre = centreOf(boundingBox);
        return centre.x - kSpreadRadius <= clip.maxX && centre.x + kSpreadRadius >= clip.minX &&
               centre.z - kSpreadRadius <= clip.maxZ && centre.z + kSpreadRadius >= clip.minZ;
    }
};

/// The neighbourhood the one global placement runs against, before its writes are clipped per chunk.
struct NetherRuinedPortalGenerator::PortalWorld {
    std::map<std::pair<std::int32_t, std::int32_t>, Chunk>* chunks = nullptr;
    const BaseStateAt* baseStateAt = nullptr;
    /// Insertion-ordered, because a replay must be applied in the order the writes happened.
    std::vector<std::pair<StructurePos, std::int32_t>> writes;
    std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::size_t> writeIndex;
    std::vector<PortalBlockEntity> blockEntities;

    [[nodiscard]] std::int32_t stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        const auto found = writeIndex.find({x, y, z});
        if (found != writeIndex.end()) {
            return writes[found->second].second;
        }
        if (y < 0 || y >= kNetherGenerationDepth) {
            return registry::blocks::air().stateId;
        }
        const auto chunk = chunks->find({floorDiv(x, 16), floorDiv(z, 16)});
        if (chunk == chunks->end()) {
            return (*baseStateAt)(x, y, z);
        }
        return chunk->second.blockStateId(x & 15, y, z & 15);
    }

    void setBlock(StructurePos position, std::int32_t stateId) {
        if (position.y < kNetherHeight.minY() || position.y > kNetherHeight.maxY()) {
            return;
        }
        const auto key = std::tuple{position.x, position.y, position.z};
        const auto found = writeIndex.find(key);
        if (found != writeIndex.end()) {
            writes[found->second].second = stateId;
            return;
        }
        writeIndex.emplace(key, writes.size());
        writes.emplace_back(position, stateId);
    }

    [[nodiscard]] std::int32_t surfaceY(std::int32_t x, std::int32_t z) const {
        for (std::int32_t y = kNetherGenerationDepth - 1; y >= 0; --y) {
            if (!registry::blocks::isAir(stateAt(x, y, z))) {
                return y;
            }
        }
        return -1;
    }
};

NetherRuinedPortalGenerator::NetherRuinedPortalGenerator(const NetherBiomeSource& biomeSource,
                                                         structure::TemplateLoader templateLoader,
                                                         BaseStateAt baseStateAt,
                                                         PlaceEarlierBastions placeEarlierBastions)
    : biomeSource_(&biomeSource), templateLoader_(std::move(templateLoader)), baseStateAt_(std::move(baseStateAt)),
      placeEarlierBastions_(std::move(placeEarlierBastions)) {}

std::int32_t NetherRuinedPortalGenerator::findSuitableY(random::RandomSource& random, bool airPocket,
                                                        const StructureBoundingBox& boundingBox) const {
    std::int32_t selectedY = 0;
    if (airPocket) {
        selectedY = 32 + random.nextInt(69);
    } else if (random.nextFloat() < 0.5F) {
        selectedY = 27 + random.nextInt(3);
    } else {
        selectedY = 29 + random.nextInt(72);
    }
    const std::array<std::pair<std::int32_t, std::int32_t>, 4> corners{
        std::pair{boundingBox.minX, boundingBox.minZ}, std::pair{boundingBox.maxX, boundingBox.minZ},
        std::pair{boundingBox.minX, boundingBox.maxZ}, std::pair{boundingBox.maxX, boundingBox.maxZ}};
    std::int32_t y = selectedY;
    while (y > kMinProjectedY) {
        std::int32_t solidCorners = 0;
        for (const auto& [x, z] : corners) {
            if (!registry::blocks::isAir(baseStateAt_(x, y, z)) && ++solidCorners == 3) {
                return y;
            }
        }
        --y;
    }
    return y;
}

std::optional<NetherRuinedPortalGenerator::PortalStart>
NetherRuinedPortalGenerator::createStart(std::int64_t seed, std::int32_t candidateChunkX,
                                         std::int32_t candidateChunkZ) const {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, candidateChunkX, candidateChunkZ);
    const bool airPocket = random.nextFloat() < kAirPocketProbability;
    const std::string templateName =
        random.nextFloat() < kGiantPortalProbability
            ? giantPortals()[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(giantPortals().size())))]
            : portals()[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(portals().size())))];
    const structure::StructureTemplate* piece = templateLoader_(*wire::common::Identifier::parse(templateName));
    if (piece == nullptr) {
        return std::nullopt;
    }
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const StructureMirror mirror = random.nextFloat() < 0.5F ? StructureMirror::None : StructureMirror::FrontBack;
    const StructurePos pivot{piece->size().x / 2, 0, piece->size().z / 2};
    const StructurePos basePosition{candidateChunkX * 16, 0, candidateChunkZ * 16};
    const StructureBoundingBox unprojectedBox = piece->getBoundingBox(basePosition, rotation, pivot, mirror);
    const std::int32_t projectedY = findSuitableY(random, airPocket, unprojectedBox);
    const StructurePos position{basePosition.x, projectedY, basePosition.z};
    if (!netherBiomes().contains(biomePath(biomeSource_->biomeAt(position.x, position.y, position.z)))) {
        return std::nullopt;
    }
    return PortalStart{.templateName = templateName,
                       .piece = piece,
                       .position = position,
                       .rotation = rotation,
                       .mirror = mirror,
                       .pivot = pivot,
                       .boundingBox = unprojectedBox.move(StructurePos{0, projectedY, 0}),
                       .airPocket = airPocket};
}

std::optional<NetherRuinedPortalStartSnapshot>
NetherRuinedPortalGenerator::startSnapshot(std::int64_t seed, std::int32_t candidateChunkX,
                                           std::int32_t candidateChunkZ) {
    const std::optional<PortalStart> start = createStart(seed, candidateChunkX, candidateChunkZ);
    if (!start.has_value()) {
        return std::nullopt;
    }
    return NetherRuinedPortalStartSnapshot{.templateName = start->templateName,
                                           .position = start->position,
                                           .rotation = start->rotation,
                                           .mirror = start->mirror,
                                           .boundingBox = start->boundingBox,
                                           .airPocket = start->airPocket};
}

void NetherRuinedPortalGenerator::fillBaseTerrain(Chunk& chunk) const {
    for (std::int32_t y = 0; y < kNetherGenerationDepth; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t stateId = baseStateAt_(chunk.chunkX() * 16 + x, y, chunk.chunkZ() * 16 + z);
                if (!registry::blocks::isAir(stateId)) {
                    chunk.setBlockStateId(x, y, z, stateId);
                }
            }
        }
    }
    for (std::int32_t z = 0; z < 16; ++z) {
        for (std::int32_t x = 0; x < 16; ++x) {
            chunk.setBlockStateId(x, 0, z, registry::blocks::bedrock().stateId);
            chunk.setBlockStateId(x, kNetherGenerationDepth - 1, z, registry::blocks::bedrock().stateId);
        }
    }
}

void NetherRuinedPortalGenerator::placeTemplate(const PortalStart& start, PortalWorld& world,
                                                random::RandomSource& random) const {
    struct Processed {
        StructurePos position;
        std::int32_t stateId = 0;
        const wire::nbt::NbtCompound* nbt = nullptr;
    };

    std::vector<Processed> processed;
    for (const structure::StructureBlockInfo& blockInfo : start.piece->blocks()) {
        const PortalState input = PortalState::from(blockInfo.stateId, blockInfo.properties);
        if (input.path == "structure_block") {
            continue;
        }
        if (!start.airPocket && input.isAir()) {
            continue;
        }
        const StructurePos relative = structure::transform(blockInfo.pos, start.mirror, start.rotation, start.pivot);
        const StructurePos worldPos{start.position.x + relative.x, start.position.y + relative.y,
                                    start.position.z + relative.z};
        const std::int32_t oldStateId = world.stateAt(worldPos.x, worldPos.y, worldPos.z);
        if (protectedBlocks().contains(pathOf(oldStateId))) {
            continue;
        }
        PortalState state = applyRuleProcessor(input, worldPos);
        state = applyBlockAgeProcessor(state, worldPos);
        if (pathOf(oldStateId) == "lava" && !state.hasFullCollisionShape()) {
            state = PortalState::of("lava");
        }
        state = applyBlackstoneReplacement(state);
        processed.push_back(
            Processed{.position = worldPos,
                      .stateId = state.transform(start.mirror, start.rotation).stateId(),
                      .nbt = blockInfo.blockEntityNbt.has_value() ? &*blockInfo.blockEntityNbt : nullptr});
    }

    for (const Processed& block : processed) {
        world.setBlock(block.position, block.stateId);
        if (pathOf(block.stateId) == "chest" && block.nbt != nullptr) {
            wire::nbt::NbtCompound data = *block.nbt;
            data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(random.nextLong()));
            if (!data.contains("components")) {
                data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
            }
            world.blockEntities.push_back(PortalBlockEntity{block.position, std::move(data)});
        }
    }

    // parity: `TemplateStructurePiece` performs this second pass after `placeInWorld`, outside the
    // processor chain, so even a processor-rejected jigsaw position receives its final state.
    for (const structure::StructureBlockInfo& blockInfo : start.piece->blocks()) {
        if (PortalState::from(blockInfo.stateId, blockInfo.properties).path != "jigsaw") {
            continue;
        }
        const StructurePos relative = structure::transform(blockInfo.pos, start.mirror, start.rotation, start.pivot);
        const StructurePos position{start.position.x + relative.x, start.position.y + relative.y,
                                    start.position.z + relative.z};
        std::string finalState = "minecraft:air";
        if (blockInfo.blockEntityNbt.has_value()) {
            finalState = std::string{blockInfo.blockEntityNbt->getString("final_state", "minecraft:air")};
        }
        const std::size_t bracket = finalState.find('[');
        if (bracket != std::string::npos) {
            finalState = finalState.substr(0, bracket);
        }
        const std::size_t colon = finalState.find(':');
        if (colon != std::string::npos) {
            finalState = finalState.substr(colon + 1);
        }
        world.setBlock(position, plainState(finalState));
    }
}

void NetherRuinedPortalGenerator::placeNetherrackOrMagma(random::RandomSource& random, PortalWorld& world,
                                                         StructurePos position) {
    world.setBlock(position, random.nextFloat() < kMagmaProbability ? magma() : netherrack());
}

void NetherRuinedPortalGenerator::addNetherrackDripColumn(random::RandomSource& random, PortalWorld& world,
                                                          StructurePos start) {
    StructurePos position = start;
    placeNetherrackOrMagma(random, world, position);
    std::int32_t remaining = 8;
    while (remaining > 0 && random.nextFloat() < 0.5F) {
        position = StructurePos{position.x, position.y - 1, position.z};
        --remaining;
        placeNetherrackOrMagma(random, world, position);
    }
}

void NetherRuinedPortalGenerator::spreadNetherrack(const StructureBoundingBox& boundingBox, PortalWorld& world,
                                                   random::RandomSource& random) {
    const StructurePos centre = centreOf(boundingBox);
    const std::int32_t averageWidth =
        ((boundingBox.maxX - boundingBox.minX + 1) + (boundingBox.maxZ - boundingBox.minZ + 1)) / 2;
    const std::int32_t distanceAdjustment = random.nextInt(std::max(1, 8 - averageWidth / 2));
    for (std::int32_t x = centre.x - kSpreadRadius; x <= centre.x + kSpreadRadius; ++x) {
        for (std::int32_t z = centre.z - kSpreadRadius; z <= centre.z + kSpreadRadius; ++z) {
            const std::int32_t adjustedDistance = std::abs(x - centre.x) + std::abs(z - centre.z) + distanceAdjustment;
            if (adjustedDistance >= kSpreadRadius) {
                continue;
            }
            if (random.nextDouble() >=
                static_cast<double>(kNetherrackByDistance[static_cast<std::size_t>(adjustedDistance)])) {
                continue;
            }
            const std::int32_t y = std::min(boundingBox.minY, world.surfaceY(x, z));
            if (std::abs(y - boundingBox.minY) > 3 || !canReplaceWithNetherrack(world.stateAt(x, y, z))) {
                continue;
            }
            placeNetherrackOrMagma(random, world, StructurePos{x, y, z});
            addNetherrackDripColumn(random, world, StructurePos{x, y - 1, z});
        }
    }
}

void NetherRuinedPortalGenerator::addNetherrackDripColumnsBelowPortal(const StructureBoundingBox& boundingBox,
                                                                      PortalWorld& world,
                                                                      random::RandomSource& random) {
    for (std::int32_t x = boundingBox.minX + 1; x < boundingBox.maxX; ++x) {
        for (std::int32_t z = boundingBox.minZ + 1; z < boundingBox.maxZ; ++z) {
            if (pathOf(world.stateAt(x, boundingBox.minY, z)) == "netherrack") {
                addNetherrackDripColumn(random, world, StructurePos{x, boundingBox.minY - 1, z});
            }
        }
    }
}

NetherRuinedPortalGenerator::PortalReplay NetherRuinedPortalGenerator::createReplay(std::int64_t seed,
                                                                                    const PortalStart& start) const {
    const StructurePos centre = centreOf(start.boundingBox);
    const std::int32_t minChunkX = floorDiv(centre.x - kSpreadRadius, 16);
    const std::int32_t maxChunkX = floorDiv(centre.x + kSpreadRadius, 16);
    const std::int32_t minChunkZ = floorDiv(centre.z - kSpreadRadius, 16);
    const std::int32_t maxChunkZ = floorDiv(centre.z + kSpreadRadius, 16);
    std::map<std::pair<std::int32_t, std::int32_t>, Chunk> chunks;
    for (std::int32_t chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX) {
        for (std::int32_t chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ) {
            Chunk chunk{chunkX, chunkZ, kNetherHeight};
            fillBaseTerrain(chunk);
            carver::carveNether(chunk, seed);
            placeEarlierBastions_(chunk, seed);
            chunks.emplace(std::pair{chunkX, chunkZ}, std::move(chunk));
        }
    }
    PortalWorld world;
    world.chunks = &chunks;
    world.baseStateAt = &baseStateAt_;
    const std::int32_t centreChunkX = floorDiv(centre.x, 16);
    const std::int32_t centreChunkZ = floorDiv(centre.z, 16);
    random::RandomSource placementRandom = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(placementRandom, seed, centreChunkX * 16, centreChunkZ * 16);
    random::setFeatureSeed(placementRandom, decorationSeed, kRuinedPortalStructureIndex, kSurfaceStructuresStep);

    placeTemplate(start, world, placementRandom);
    spreadNetherrack(start.boundingBox, world, placementRandom);
    addNetherrackDripColumnsBelowPortal(start.boundingBox, world, placementRandom);
    PortalReplay replay;
    replay.writes = std::move(world.writes);
    for (PortalBlockEntity& entity : world.blockEntities) {
        const auto found = std::find_if(
            replay.writes.begin(), replay.writes.end(),
            [&entity](const std::pair<StructurePos, std::int32_t>& write) { return write.first == entity.position; });
        const std::int32_t finalState = found == replay.writes.end() ? registry::blocks::air().stateId : found->second;
        if (pathOf(finalState) == "chest") {
            replay.blockEntities.push_back(std::move(entity));
        }
    }
    return replay;
}

const NetherRuinedPortalGenerator::PortalReplay& NetherRuinedPortalGenerator::replay(std::int64_t seed,
                                                                                     std::int32_t candidateChunkX,
                                                                                     std::int32_t candidateChunkZ,
                                                                                     const PortalStart& start) {
    const PortalCacheKey key{seed, candidateChunkX, candidateChunkZ};
    const std::scoped_lock lock{mutex_};
    const auto found = replays_.find(key);
    if (found != replays_.end()) {
        return found->second;
    }
    return replays_.emplace(key, createReplay(seed, start)).first->second;
}

void NetherRuinedPortalGenerator::applyReplay(Chunk& chunk, const PortalReplay& replay) {
    for (const auto& [position, stateId] : replay.writes) {
        if (floorDiv(position.x, 16) != chunk.chunkX() || floorDiv(position.z, 16) != chunk.chunkZ()) {
            continue;
        }
        if (position.y < chunk.verticalBounds().minY() || position.y > chunk.verticalBounds().maxY()) {
            continue;
        }
        chunk.setBlockStateId(position.x & 15, position.y, position.z & 15, stateId);
    }
    for (const PortalBlockEntity& entity : replay.blockEntities) {
        const StructurePos position = entity.position;
        if (floorDiv(position.x, 16) != chunk.chunkX() || floorDiv(position.z, 16) != chunk.chunkZ()) {
            continue;
        }
        if (position.y < chunk.verticalBounds().minY() || position.y > chunk.verticalBounds().maxY()) {
            continue;
        }
        chunk.setBlockEntity(ChunkBlockEntity{position.x & 15, position.y, position.z & 15,
                                              wire::common::Identifier::vanilla("chest"), entity.data});
    }
}

void NetherRuinedPortalGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t spacing = portalGrid().spacing();
    const std::int32_t targetCellX = floorDiv(chunk.chunkX(), spacing);
    const std::int32_t targetCellZ = floorDiv(chunk.chunkZ(), spacing);
    const StructureBoundingBox clip{chunk.chunkX() * 16,      chunk.verticalBounds().minY(), chunk.chunkZ() * 16,
                                    chunk.chunkX() * 16 + 15, chunk.verticalBounds().maxY(), chunk.chunkZ() * 16 + 15};
    for (std::int32_t cellOffsetX = -1; cellOffsetX <= 1; ++cellOffsetX) {
        for (std::int32_t cellOffsetZ = -1; cellOffsetZ <= 1; ++cellOffsetZ) {
            const auto [candidateX, candidateZ] = portalGrid().candidateChunk(
                seed, (targetCellX + cellOffsetX) * spacing, (targetCellZ + cellOffsetZ) * spacing);
            const std::optional<PortalStart> start = createStart(seed, candidateX, candidateZ);
            if (!start.has_value() || !start->effectIntersects(clip)) {
                continue;
            }
            applyReplay(chunk, replay(seed, candidateX, candidateZ, *start));
        }
    }
}

} // namespace basalt::server::world::terrain
