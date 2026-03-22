#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/structure/JigsawAssembly.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world::structure;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace rng = basalt::server::random;

namespace {

struct Connector {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::string orientation;
    std::string name;
    std::string pool;
    std::string target;
    bool rollable = true;
    std::int32_t placementPriority = 0;
    std::int32_t selectionPriority = 0;
};

const std::vector<std::string>& orientations() {
    static const std::vector<std::string> names = {"north_up", "south_up",   "west_up",  "east_up",
                                                   "up_north", "down_north", "down_east"};
    return names;
}

/// A synthetic piece: a box of stone with one jigsaw block per connector. Nothing here comes from a
/// Mojang structure file; what is under test is the assembly, not the buildings.
[[nodiscard]] StructureTemplate piece(std::int32_t sizeX, std::int32_t sizeY, std::int32_t sizeZ,
                                      const std::vector<Connector>& connectors) {
    NbtList palette{NbtType::Compound};
    {
        NbtCompound stone;
        stone.set("Name", NbtTag::ofString("minecraft:stone"));
        palette.push(NbtTag::ofCompound(std::move(stone)));
    }
    for (const std::string& orientation : orientations()) {
        NbtCompound properties;
        properties.set("orientation", NbtTag::ofString(orientation));
        NbtCompound jigsaw;
        jigsaw.set("Name", NbtTag::ofString("minecraft:jigsaw"));
        jigsaw.set("Properties", NbtTag::ofCompound(std::move(properties)));
        palette.push(NbtTag::ofCompound(std::move(jigsaw)));
    }

    NbtList blocks{NbtType::Compound};
    const auto isConnector = [&connectors](std::int32_t x, std::int32_t y, std::int32_t z) {
        for (const Connector& connector : connectors) {
            if (connector.x == x && connector.y == y && connector.z == z) {
                return true;
            }
        }
        return false;
    };
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                if (isConnector(x, y, z)) {
                    continue;
                }
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                block.set("state", NbtTag::ofInt(0));
                blocks.push(NbtTag::ofCompound(std::move(block)));
            }
        }
    }
    for (const Connector& connector : connectors) {
        std::int32_t stateIndex = 1;
        for (std::size_t index = 0; index < orientations().size(); ++index) {
            if (orientations()[index] == connector.orientation) {
                stateIndex = 1 + static_cast<std::int32_t>(index);
            }
        }
        NbtCompound data;
        data.set("name", NbtTag::ofString(connector.name));
        data.set("pool", NbtTag::ofString(connector.pool));
        data.set("target", NbtTag::ofString(connector.target));
        data.set("final_state", NbtTag::ofString("minecraft:air"));
        data.set("joint", NbtTag::ofString(connector.rollable ? "rollable" : "aligned"));
        data.set("placement_priority", NbtTag::ofInt(connector.placementPriority));
        data.set("selection_priority", NbtTag::ofInt(connector.selectionPriority));
        NbtCompound block;
        block.set("pos", NbtTag::ofList(NbtList{
                             NbtType::Int,
                             {NbtTag::ofInt(connector.x), NbtTag::ofInt(connector.y), NbtTag::ofInt(connector.z)}}));
        block.set("state", NbtTag::ofInt(stateIndex));
        block.set("nbt", NbtTag::ofCompound(std::move(data)));
        blocks.push(NbtTag::ofCompound(std::move(block)));
    }

    NbtCompound nbt;
    nbt.set("size",
            NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(sizeX), NbtTag::ofInt(sizeY), NbtTag::ofInt(sizeZ)}}));
    nbt.set("palette", NbtTag::ofList(std::move(palette)));
    nbt.set("blocks", NbtTag::ofList(std::move(blocks)));
    StructureTemplate built;
    built.load(nbt);
    return built;
}

const std::map<std::string, StructureTemplate, std::less<>>& templates() {
    static const std::map<std::string, StructureTemplate, std::less<>> built = [] {
        std::map<std::string, StructureTemplate, std::less<>> map;
        map.emplace("minecraft:test/start",
                    piece(9, 7, 9,
                          {// Two connectors of equal priority, so the queue's insertion-order tie-break is
                           // the only thing deciding which one is expanded first.
                           Connector{.x = 8,
                                     .y = 1,
                                     .z = 4,
                                     .orientation = "east_up",
                                     .name = "start_a",
                                     .pool = "minecraft:test/wings",
                                     .target = "wing_in"},
                           Connector{.x = 0,
                                     .y = 1,
                                     .z = 4,
                                     .orientation = "west_up",
                                     .name = "start_b",
                                     .pool = "minecraft:test/wings",
                                     .target = "wing_in"},
                           // Aligned and vertical: the joint has to match tops, and the front carries a dy
                           // that the offset between the two pieces depends on.
                           Connector{.x = 4,
                                     .y = 6,
                                     .z = 4,
                                     .orientation = "up_north",
                                     .name = "start_c",
                                     .pool = "minecraft:test/towers",
                                     .target = "tower_in",
                                     .rollable = false,
                                     .placementPriority = 1},
                           // Points into its own box, so its children use the piece's own free space rather
                           // than the shared one. Small enough to fit there, which is the point: in the
                           // shared space it would collide with its own parent.
                           Connector{.x = 4,
                                     .y = 1,
                                     .z = 4,
                                     .orientation = "east_up",
                                     .name = "start_d",
                                     .pool = "minecraft:test/pebbles",
                                     .target = "pebble_in"}}));
        map.emplace("minecraft:test/wing_big", piece(5, 5, 5,
                                                     {Connector{.x = 0,
                                                                .y = 1,
                                                                .z = 2,
                                                                .orientation = "west_up",
                                                                .name = "wing_in",
                                                                .pool = "minecraft:test/leaves",
                                                                .target = "leaf_in"},
                                                      Connector{.x = 4,
                                                                .y = 1,
                                                                .z = 2,
                                                                .orientation = "east_up",
                                                                .name = "wing_out",
                                                                .pool = "minecraft:test/leaves",
                                                                .target = "leaf_in"},
                                                      // Points back into its own box, which is the only shape the
                                                      // expansion hack grows a piece for.
                                                      Connector{.x = 2,
                                                                .y = 1,
                                                                .z = 2,
                                                                .orientation = "east_up",
                                                                .name = "wing_inner",
                                                                .pool = "minecraft:test/leaves",
                                                                .target = "leaf_in"}}));
        map.emplace("minecraft:test/wing_small", piece(3, 5, 3,
                                                       {Connector{.x = 0,
                                                                  .y = 1,
                                                                  .z = 1,
                                                                  .orientation = "west_up",
                                                                  .name = "wing_in",
                                                                  .pool = "minecraft:empty",
                                                                  .target = "empty"}}));
        // Only reachable through the wings pool's fallback.
        map.emplace("minecraft:test/wing_spare", piece(3, 3, 3,
                                                       {Connector{.x = 0,
                                                                  .y = 1,
                                                                  .z = 1,
                                                                  .orientation = "west_up",
                                                                  .name = "wing_in",
                                                                  .pool = "minecraft:empty",
                                                                  .target = "empty"}}));
        map.emplace("minecraft:test/tower_north", piece(3, 9, 3,
                                                        {Connector{.x = 1,
                                                                   .y = 0,
                                                                   .z = 1,
                                                                   .orientation = "down_north",
                                                                   .name = "tower_in",
                                                                   .pool = "minecraft:empty",
                                                                   .target = "empty"}}));
        // The same shape with the wrong top, which an aligned joint refuses.
        map.emplace("minecraft:test/tower_east", piece(3, 5, 3,
                                                       {Connector{.x = 1,
                                                                  .y = 0,
                                                                  .z = 1,
                                                                  .orientation = "down_east",
                                                                  .name = "tower_in",
                                                                  .pool = "minecraft:empty",
                                                                  .target = "empty"}}));
        // Tall on purpose: the expansion hack grows a piece to fit its children, and a child no
        // taller than its parent leaves nothing to grow.
        map.emplace("minecraft:test/leaf", piece(3, 9, 3,
                                                 {Connector{.x = 0,
                                                            .y = 1,
                                                            .z = 1,
                                                            .orientation = "west_up",
                                                            .name = "leaf_in",
                                                            .pool = "minecraft:empty",
                                                            .target = "empty"}}));
        map.emplace("minecraft:test/pebble", piece(2, 2, 2,
                                                   {Connector{.x = 0,
                                                              .y = 0,
                                                              .z = 0,
                                                              .orientation = "west_up",
                                                              .name = "pebble_in",
                                                              .pool = "minecraft:empty",
                                                              .target = "empty"}}));
        return map;
    }();
    return built;
}

[[nodiscard]] StructurePoolElement single(std::string location,
                                          StructureProjection projection = StructureProjection::Rigid) {
    return StructurePoolElement::legacySingle(std::move(location), StructureProcessorList{}, projection);
}

const std::map<std::string, StructureTemplatePool, std::less<>>& pools() {
    static const std::map<std::string, StructureTemplatePool, std::less<>> built = [] {
        std::map<std::string, StructureTemplatePool, std::less<>> map;
        map.emplace("minecraft:empty", StructureTemplatePool{"minecraft:empty", {{StructurePoolElement::empty(), 1}}});
        map.emplace("minecraft:test/start",
                    StructureTemplatePool{"minecraft:empty", {{single("minecraft:test/start"), 1}}});
        map.emplace("minecraft:test/wings", StructureTemplatePool{"minecraft:test/wings_spare",
                                                                  {{single("minecraft:test/wing_big"), 3},
                                                                   {single("minecraft:test/wing_small"), 1}}});
        map.emplace("minecraft:test/wings_spare",
                    StructureTemplatePool{"minecraft:empty", {{single("minecraft:test/wing_spare"), 1}}});
        map.emplace("minecraft:test/towers", StructureTemplatePool{"minecraft:empty",
                                                                   {{single("minecraft:test/tower_east"), 3},
                                                                    {single("minecraft:test/tower_north"), 1}}});
        map.emplace("minecraft:test/leaves",
                    StructureTemplatePool{"minecraft:empty", {{single("minecraft:test/leaf"), 1}}});
        map.emplace("minecraft:test/pebbles",
                    StructureTemplatePool{"minecraft:empty", {{single("minecraft:test/pebble"), 1}}});
        return map;
    }();
    return built;
}

[[nodiscard]] std::string describe(const PlacedStructurePiece& piece) {
    const StructureBoundingBox& box = piece.boundingBox;
    const std::string location =
        piece.element.kind() == StructurePoolElement::Kind::LegacySingle ? piece.element.location() : "empty";
    const auto rotationName = [](StructureRotation rotation) -> std::string {
        switch (rotation) {
        case StructureRotation::None:
            return "NONE";
        case StructureRotation::Clockwise90:
            return "CLOCKWISE_90";
        case StructureRotation::Clockwise180:
            return "CLOCKWISE_180";
        case StructureRotation::Counterclockwise90:
        default:
            return "COUNTERCLOCKWISE_90";
        }
    };
    return location + "@" + std::to_string(piece.position.x) + "," + std::to_string(piece.position.y) + "," +
           std::to_string(piece.position.z) + "/" + rotationName(piece.rotation) + "/" + std::to_string(box.minX) +
           "," + std::to_string(box.minY) + "," + std::to_string(box.minZ) + "," + std::to_string(box.maxX) + "," +
           std::to_string(box.maxY) + "," + std::to_string(box.maxZ) + "/" + std::to_string(piece.groundLevelDelta) +
           "/" + std::to_string(piece.junctions.size());
}

struct Run {
    std::size_t pieces = 0;
    std::string first;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
    /// The horizontal centre of the start piece's box.
    std::int32_t centreX = 0;
    std::int32_t centreZ = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::int32_t maxDepth, bool projectStartToHeightmap,
                      std::int32_t maxDistanceFromCenter, bool useExpansionHack, std::string startJigsawName = {},
                      std::function<bool(const StructurePos&)> acceptStart = {}) {
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(seed));
    const JigsawWorld world{.poolLookup = [](std::string_view name) -> const StructureTemplatePool* {
                                const auto found = pools().find(name);
                                return found == pools().end() ? nullptr : &found->second;
                            },
                            .templateLoader = [](std::string_view name) -> const StructureTemplate* {
                                const auto found = templates().find(name);
                                return found == templates().end() ? nullptr : &found->second;
                            },
                            .heightAt = [](std::int32_t, std::int32_t) { return 64; }};
    const JigsawRequest request{.startPool = "minecraft:test/start",
                                .startPosition = StructurePos{0, 0, 0},
                                .maxDepth = maxDepth,
                                .projectStartToHeightmap = projectStartToHeightmap,
                                .maxDistanceFromCenter = maxDistanceFromCenter,
                                .useExpansionHack = useExpansionHack,
                                .startJigsawName = std::move(startJigsawName),
                                .acceptStart = std::move(acceptStart)};
    const std::vector<PlacedStructurePiece> pieces = assembleJigsaw(world, request, random);

    std::uint64_t checksum = 14695981039346656037ULL;
    for (const PlacedStructurePiece& piece : pieces) {
        for (const char letter : describe(piece)) {
            checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(letter))) * 1099511628211ULL;
        }
        for (const PlacedJigsawJunction& junction : piece.junctions) {
            for (const std::int32_t value :
                 {junction.sourceX, junction.sourceGroundY, junction.sourceZ, junction.deltaY,
                  static_cast<std::int32_t>(junction.destinationProjection)}) {
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(value))) * 1099511628211ULL;
            }
        }
    }
    Run result;
    result.pieces = pieces.size();
    result.first = pieces.empty() ? "null" : describe(pieces.front());
    result.checksum = static_cast<std::int64_t>(checksum);
    result.after = random.nextInt();
    if (!pieces.empty()) {
        result.centreX = (pieces.front().boundingBox.minX + pieces.front().boundingBox.maxX) / 2;
        result.centreZ = (pieces.front().boundingBox.minZ + pieces.front().boundingBox.maxZ) / 2;
    }
    return result;
}

void check(const Run& result, std::size_t pieces, std::string_view first, std::int64_t checksum, std::int32_t after) {
    CHECK(result.pieces == pieces);
    CHECK(result.first == first);
    CHECK(result.checksum == checksum);
    CHECK(result.after == after);
}

} // namespace

TEST_CASE("the jigsaw assembly builds what the frozen kotlin builds") {
    SUBCASE("depth0") {
        check(run(4321, 0, false, 80, false), 1, "minecraft:test/start@0,-1,0/CLOCKWISE_90/-8,-1,0,0,5,8/1/0",
              1229859946027079810LL, 1055231144);
    }

    SUBCASE("depth1") {
        check(run(4321, 1, false, 80, false), 5, "minecraft:test/start@0,-1,0/CLOCKWISE_90/-8,-1,0,0,5,8/1/4",
              -7648910774950557287LL, 1937268796);
    }

    SUBCASE("depth3") {
        check(run(4321, 3, false, 80, false), 6, "minecraft:test/start@0,-1,0/CLOCKWISE_90/-8,-1,0,0,5,8/1/4",
              548243346743260815LL, 1238095718);
    }

    SUBCASE("projected") {
        check(run(4321, 3, true, 80, false), 6, "minecraft:test/start@0,63,0/CLOCKWISE_90/-8,63,0,0,69,8/1/4",
              -7984718719247978836LL, 1238095718);
    }

    SUBCASE("expanded") {
        check(run(4321, 3, false, 80, true), 6, "minecraft:test/start@0,-1,0/CLOCKWISE_90/-8,-1,0,0,5,8/1/4",
              8148756638381486121LL, 1238095718);
    }

    SUBCASE("tight") {
        check(run(4321, 3, false, 6, false), 3, "minecraft:test/start@0,-1,0/CLOCKWISE_90/-8,-1,0,0,5,8/1/2",
              8198620815058024789LL, -505343888);
    }

    SUBCASE("anchored") {
        check(run(4321, 2, false, 80, false, "start_b"), 5,
              "minecraft:test/start@4,-2,0/CLOCKWISE_90/-4,-2,0,4,4,8/1/4", -7659173472241152887LL, 2069649401);
    }

    SUBCASE("otherSeed") {
        check(run(99, 3, false, 80, false), 7, "minecraft:test/start@0,-1,0/NONE/0,-1,0,8,5,8/1/4",
              4674818880423394264LL, -283459008);
    }
}

// parity: `Structure.isValidBiome` judges the stub `JigsawPlacement.addPieces` returns: the centre of
// the start piece's box, at the anchor's height, which for an anchored start is the start height.
TEST_CASE("a jigsaw start is judged at its stub before any child is placed") {
    for (const bool anchored : {false, true}) {
        CAPTURE(anchored);
        const std::string anchor = anchored ? "start_b" : "";
        std::optional<StructurePos> stub;
        const Run accepted = run(4321, 2, !anchored, 80, false, anchor, [&stub](const StructurePos& at) {
            stub = at;
            return true;
        });
        REQUIRE(stub.has_value());
        CHECK(accepted.pieces > 1);
        const Run plain = run(4321, 2, !anchored, 80, false, anchor);
        CHECK(accepted.checksum == plain.checksum);
        CHECK(accepted.after == plain.after);
        CHECK(stub->x == accepted.centreX);
        CHECK(stub->z == accepted.centreZ);
        CHECK(stub->y == (anchored ? 0 : 64));

        const Run refused = run(4321, 2, !anchored, 80, false, anchor, [](const StructurePos&) { return false; });
        CHECK(refused.pieces == 0);
    }
}
