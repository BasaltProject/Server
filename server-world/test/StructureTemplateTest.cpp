#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <doctest.h>

#include <bit>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::structure;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] NbtTag intList(std::int32_t x, std::int32_t y, std::int32_t z) {
    return NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}});
}

[[nodiscard]] NbtCompound paletteEntry(std::string name,
                                       const std::vector<std::pair<std::string, std::string>>& properties = {}) {
    NbtCompound entry;
    entry.set("Name", NbtTag::ofString(std::move(name)));
    if (!properties.empty()) {
        NbtCompound tag;
        for (const auto& [key, value] : properties) {
            tag.set(key, NbtTag::ofString(value));
        }
        entry.set("Properties", NbtTag::ofCompound(std::move(tag)));
    }
    return entry;
}

[[nodiscard]] NbtCompound blockEntry(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t state,
                                     const NbtCompound* nbt = nullptr) {
    NbtCompound entry;
    entry.set("pos", intList(x, y, z));
    entry.set("state", NbtTag::ofInt(state));
    if (nbt != nullptr) {
        entry.set("nbt", NbtTag::ofCompound(*nbt));
    }
    return entry;
}

/// Every property family the transform touches, in one template. Built here rather than loaded from
/// a file: no Mojang asset goes near the repository, and a synthetic one can carry them all at once.
[[nodiscard]] NbtCompound templateNbt(std::int32_t palettes = 1) {
    NbtList palette{NbtType::Compound};
    palette.push(NbtTag::ofCompound(paletteEntry("minecraft:stone")));
    palette.push(NbtTag::ofCompound(
        paletteEntry("minecraft:oak_stairs",
                     {{"facing", "north"}, {"half", "bottom"}, {"shape", "outer_left"}, {"waterlogged", "false"}})));
    palette.push(
        NbtTag::ofCompound(paletteEntry("minecraft:rail", {{"shape", "north_east"}, {"waterlogged", "false"}})));
    palette.push(NbtTag::ofCompound(paletteEntry(
        "minecraft:oak_door",
        {{"facing", "east"}, {"half", "lower"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}})));
    palette.push(NbtTag::ofCompound(paletteEntry(
        "minecraft:oak_fence",
        {{"north", "true"}, {"east", "false"}, {"south", "false"}, {"west", "true"}, {"waterlogged", "false"}})));
    palette.push(NbtTag::ofCompound(paletteEntry("minecraft:oak_sign", {{"rotation", "3"}, {"waterlogged", "false"}})));
    palette.push(NbtTag::ofCompound(paletteEntry("minecraft:jigsaw", {{"orientation", "north_up"}})));
    palette.push(NbtTag::ofCompound(
        paletteEntry("minecraft:chest", {{"facing", "west"}, {"type", "single"}, {"waterlogged", "false"}})));
    palette.push(NbtTag::ofCompound(paletteEntry("minecraft:structure_block", {{"mode", "data"}})));
    palette.push(NbtTag::ofCompound(paletteEntry("minecraft:oak_log", {{"axis", "x"}})));
    // A second sign past the halfway mark: the rotation property is read as signed, and anything at
    // or below 8 never notices.
    palette.push(
        NbtTag::ofCompound(paletteEntry("minecraft:oak_sign", {{"rotation", "13"}, {"waterlogged", "false"}})));
    // An inner corner stair: only a left-right mirror swaps those halves, and the outer corner above
    // cannot show that.
    palette.push(NbtTag::ofCompound(
        paletteEntry("minecraft:oak_stairs",
                     {{"facing", "south"}, {"half", "top"}, {"shape", "inner_left"}, {"waterlogged", "false"}})));
    // The same inner corner facing east: a front-back mirror crosses that axis and a left-right one
    // does not, which is where the two halves stop agreeing.
    palette.push(NbtTag::ofCompound(
        paletteEntry("minecraft:oak_stairs",
                     {{"facing", "east"}, {"half", "top"}, {"shape", "inner_left"}, {"waterlogged", "false"}})));

    NbtCompound jigsawNbt;
    jigsawNbt.set("id", NbtTag::ofString("minecraft:jigsaw"));
    jigsawNbt.set("joint", NbtTag::ofString("aligned"));
    jigsawNbt.set("name", NbtTag::ofString("minecraft:test_name"));
    jigsawNbt.set("pool", NbtTag::ofString("minecraft:test_pool"));
    jigsawNbt.set("target", NbtTag::ofString("minecraft:test_target"));
    jigsawNbt.set("final_state", NbtTag::ofString("minecraft:oak_planks"));
    jigsawNbt.set("placement_priority", NbtTag::ofInt(3));
    jigsawNbt.set("selection_priority", NbtTag::ofInt(7));
    NbtCompound chestNbt;
    chestNbt.set("id", NbtTag::ofString("minecraft:chest"));
    NbtCompound markerNbt;
    markerNbt.set("id", NbtTag::ofString("minecraft:structure_block"));
    markerNbt.set("mode", NbtTag::ofString("DATA"));
    markerNbt.set("metadata", NbtTag::ofString("chest_east"));

    NbtList blocks{NbtType::Compound};
    blocks.push(NbtTag::ofCompound(blockEntry(0, 0, 0, 0)));
    blocks.push(NbtTag::ofCompound(blockEntry(1, 0, 0, 1)));
    blocks.push(NbtTag::ofCompound(blockEntry(2, 0, 0, 2)));
    blocks.push(NbtTag::ofCompound(blockEntry(0, 1, 0, 3)));
    blocks.push(NbtTag::ofCompound(blockEntry(1, 1, 0, 4)));
    blocks.push(NbtTag::ofCompound(blockEntry(2, 1, 0, 5)));
    blocks.push(NbtTag::ofCompound(blockEntry(0, 0, 1, 6, &jigsawNbt)));
    blocks.push(NbtTag::ofCompound(blockEntry(1, 0, 1, 7, &chestNbt)));
    blocks.push(NbtTag::ofCompound(blockEntry(2, 0, 1, 8, &markerNbt)));
    blocks.push(NbtTag::ofCompound(blockEntry(0, 1, 1, 9)));
    blocks.push(NbtTag::ofCompound(blockEntry(1, 1, 1, 10)));
    blocks.push(NbtTag::ofCompound(blockEntry(2, 1, 1, 11)));
    blocks.push(NbtTag::ofCompound(blockEntry(0, 0, 2, 12)));
    blocks.push(NbtTag::ofCompound(blockEntry(1, 0, 2, 2)));
    blocks.push(NbtTag::ofCompound(blockEntry(2, 0, 2, 4)));

    NbtCompound entityNbt;
    entityNbt.set("id", NbtTag::ofString("minecraft:armor_stand"));
    entityNbt.set("Rotation",
                  NbtTag::ofList(NbtList{NbtType::Float, {NbtTag::ofFloat(45.0F), NbtTag::ofFloat(10.0F)}}));
    NbtCompound entity;
    entity.set("pos", NbtTag::ofList(NbtList{NbtType::Double,
                                             {NbtTag::ofDouble(1.5), NbtTag::ofDouble(1.0), NbtTag::ofDouble(0.5)}}));
    entity.set("blockPos", intList(1, 1, 0));
    entity.set("nbt", NbtTag::ofCompound(std::move(entityNbt)));

    NbtCompound nbt;
    nbt.set("size", intList(3, 2, 3));
    nbt.set("blocks", NbtTag::ofList(std::move(blocks)));
    if (palettes > 1) {
        // A second palette that differs only in its first state, which is what the draw picks.
        NbtList other{NbtType::Compound};
        for (std::size_t index = 0; index < palette.size(); ++index) {
            other.push(index == 0 ? NbtTag::ofCompound(paletteEntry("minecraft:cobblestone")) : palette[index]);
        }
        nbt.set("palettes",
                NbtTag::ofList(
                    NbtList{NbtType::List, {NbtTag::ofList(std::move(palette)), NbtTag::ofList(std::move(other))}}));
    } else {
        nbt.set("palette", NbtTag::ofList(std::move(palette)));
    }
    nbt.set("entities", NbtTag::ofList(NbtList{NbtType::Compound, {NbtTag::ofCompound(std::move(entity))}}));
    return nbt;
}

[[nodiscard]] StructureTemplate loaded(std::int32_t palettes = 1) {
    StructureTemplate built;
    built.load(templateNbt(palettes));
    return built;
}

[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y <= 64; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

struct Folded {
    std::int64_t checksum = 0;
    std::int32_t placed = 0;
    std::size_t blockEntities = 0;
};

[[nodiscard]] Folded fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Folded result;
    for (std::int32_t y = 58; y <= 80; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::air().stateId && id != registry::blocks::stone().stateId) {
                    ++result.placed;
                }
            }
        }
    }
    // The entities travel with the placement: their transformed position and yaw are the only
    // observable the block fold would otherwise miss.
    for (const ChunkGeneratedEntity& entity : chunk.generatedEntities()) {
        checksum = (checksum ^ static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(entity.x))) * 1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(entity.y))) * 1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(entity.z))) * 1099511628211ULL;
        checksum = (checksum ^
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(entity.yaw)))) *
                   1099511628211ULL;
        checksum = (checksum ^
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(entity.pitch)))) *
                   1099511628211ULL;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    result.blockEntities = chunk.blockEntities().size();
    return result;
}

[[nodiscard]] std::string transformRows(const StructureTemplate& built, StructureMirror mirror,
                                        StructureRotation rotation) {
    std::string rows;
    for (const StructureBlockInfo& block : built.blocks()) {
        if (!rows.empty()) {
            rows += ",";
        }
        rows +=
            std::to_string(StructureTemplate::transformedStateId(block.stateId, block.properties, mirror, rotation));
    }
    return rows;
}

struct Placement {
    std::int32_t placed = 0;
    std::size_t entities = 0;
    std::size_t blockEntities = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Placement place(StructureMirror mirror, StructureRotation rotation) {
    const StructureTemplate built = loaded();
    Chunk chunk = groundChunk(0, 0);
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(5309LL));
    TemplatePlacement placement;
    placement.position = StructurePos{4, 65, 6};
    placement.rotation = rotation;
    placement.mirror = mirror;
    placement.pivot = StructurePos{0, 0, 0};
    placement.clipTo = StructureBoundingBox{0, -64, 0, 15, 319, 15};
    placement.random = &random;
    built.placeInto(chunk, placement);
    const Folded folded = fold(chunk);
    Placement result;
    result.placed = folded.placed;
    result.entities = chunk.generatedEntities().size();
    result.blockEntities = folded.blockEntities;
    result.deferred = chunk.takeGeneratedBlockWrites().size();
    result.checksum = folded.checksum;
    result.after = random.nextInt();
    return result;
}

struct Query {
    std::string box;
    std::size_t jigsaws = 0;
    std::string jigsaw;
    std::size_t markers = 0;
    std::string marker;
};

[[nodiscard]] std::string upper(std::string_view name) {
    std::string out{name};
    for (char& character : out) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 32);
        }
    }
    return out;
}

[[nodiscard]] Query query(StructureRotation rotation) {
    const StructureTemplate built = loaded();
    const StructurePos position{10, 64, 20};
    const StructurePos pivot{0, 0, 0};
    const StructureBoundingBox box = built.getBoundingBox(position, rotation, pivot, StructureMirror::None);
    const std::vector<PlacedJigsaw> jigsaws = built.jigsawBlocks(position, rotation, StructureMirror::None, pivot);
    const std::vector<StructureDataMarker> markers =
        built.dataMarkers(position, rotation, StructureMirror::None, pivot);
    Query result;
    result.box = std::to_string(box.minX) + "," + std::to_string(box.minY) + "," + std::to_string(box.minZ) + "," +
                 std::to_string(box.maxX) + "," + std::to_string(box.maxY) + "," + std::to_string(box.maxZ);
    result.jigsaws = jigsaws.size();
    if (!jigsaws.empty()) {
        const PlacedJigsaw& jigsaw = jigsaws.front();
        result.jigsaw = std::to_string(jigsaw.pos.x) + "," + std::to_string(jigsaw.pos.y) + "," +
                        std::to_string(jigsaw.pos.z) + "/" + upper(serializedName(jigsaw.front)) + "/" +
                        upper(serializedName(jigsaw.top)) + "/" + (jigsaw.rollable ? "true" : "false") + "/" +
                        jigsaw.name + "/" + jigsaw.pool + "/" + jigsaw.target + "/" + jigsaw.finalStateName + "/" +
                        std::to_string(jigsaw.placementPriority) + "/" + std::to_string(jigsaw.selectionPriority);
    } else {
        result.jigsaw = "-";
    }
    result.markers = markers.size();
    if (!markers.empty()) {
        const StructureDataMarker& marker = markers.front();
        result.marker = std::to_string(marker.pos.x) + "," + std::to_string(marker.pos.y) + "," +
                        std::to_string(marker.pos.z) + "/" + marker.metadata;
    } else {
        result.marker = "-";
    }
    return result;
}

} // namespace

TEST_CASE("the structure template loads what the frozen kotlin loads") {
    const StructureTemplate built = loaded();
    CHECK(built.size().x == 3);
    CHECK(built.size().y == 2);
    CHECK(built.size().z == 3);
    CHECK(built.blocks().size() == 15);
    CHECK(built.entities().size() == 1);
    CHECK(built.paletteCount() == 1);

    const StructureTemplate many = loaded(2);
    CHECK(many.paletteCount() == 2);
    CHECK(many.blocks().size() == 15);

    std::string states;
    for (const StructureBlockInfo& block : built.blocks()) {
        if (!states.empty()) {
            states += ",";
        }
        states += std::to_string(block.pos.x) + std::to_string(block.pos.y) + std::to_string(block.pos.z) + ":" +
                  std::to_string(block.stateId);
    }
    CHECK(states ==
          "000:1,100:5480,200:7334,010:7302,110:8657,210:6898,001:25093,101:5556,201:25082,011:139,111:6918,211"
          ":5486,002:5526,102:7334,202:8657");
}

TEST_CASE("the state transform matches the frozen kotlin") {
    const StructureTemplate template_ = loaded();

    SUBCASE("NONE_NONE") {
        CHECK(transformRows(template_, StructureMirror::None, StructureRotation::None) ==
              "1,5480,7334,7302,8657,6898,25093,5556,25082,139,6918,5486,5526,7334,8657");
    }

    SUBCASE("NONE_CLOCKWISE_90") {
        CHECK(transformRows(template_, StructureMirror::None, StructureRotation::Clockwise90) ==
              "1,5540,7328,7270,8642,6906,25093,5544,25082,141,6894,5506,5486,7328,8642");
    }

    SUBCASE("NONE_CLOCKWISE_180") {
        CHECK(transformRows(template_, StructureMirror::None, StructureRotation::Clockwise180) ==
              "1,5500,7330,7286,8646,6914,25093,5562,25082,139,6902,5466,5506,7330,8646");
    }

    SUBCASE("NONE_COUNTERCLOCKWISE_90") {
        CHECK(transformRows(template_, StructureMirror::None, StructureRotation::Counterclockwise90) ==
              "1,5520,7332,7254,8661,6922,25093,5550,25082,141,6910,5526,5466,7332,8661");
    }

    SUBCASE("LEFT_RIGHT_NONE") {
        CHECK(transformRows(template_, StructureMirror::LeftRight, StructureRotation::None) ==
              "1,5502,7328,7306,8661,6902,25093,5556,25082,139,6914,5468,5526,7328,8661");
    }

    SUBCASE("LEFT_RIGHT_CLOCKWISE_90") {
        CHECK(transformRows(template_, StructureMirror::LeftRight, StructureRotation::Clockwise90) ==
              "1,5522,7330,7274,8657,6910,25093,5544,25082,141,6922,5528,5486,7330,8657");
    }

    SUBCASE("LEFT_RIGHT_CLOCKWISE_180") {
        CHECK(transformRows(template_, StructureMirror::LeftRight, StructureRotation::Clockwise180) ==
              "1,5482,7332,7290,8642,6918,25093,5562,25082,139,6898,5488,5506,7332,8642");
    }

    SUBCASE("LEFT_RIGHT_COUNTERCLOCKWISE_90") {
        CHECK(transformRows(template_, StructureMirror::LeftRight, StructureRotation::Counterclockwise90) ==
              "1,5542,7334,7258,8646,6894,25093,5550,25082,141,6906,5508,5466,7334,8646");
    }

    SUBCASE("FRONT_BACK_NONE") {
        CHECK(transformRows(template_, StructureMirror::FrontBack, StructureRotation::None) ==
              "1,5480,7332,7290,8642,6918,25093,5562,25082,139,6898,5486,5506,7332,8642");
    }

    SUBCASE("FRONT_BACK_CLOCKWISE_90") {
        CHECK(transformRows(template_, StructureMirror::FrontBack, StructureRotation::Clockwise90) ==
              "1,5540,7334,7258,8646,6894,25093,5550,25082,141,6906,5506,5466,7334,8646");
    }

    SUBCASE("FRONT_BACK_CLOCKWISE_180") {
        CHECK(transformRows(template_, StructureMirror::FrontBack, StructureRotation::Clockwise180) ==
              "1,5500,7328,7306,8661,6902,25093,5556,25082,139,6914,5466,5526,7328,8661");
    }

    SUBCASE("FRONT_BACK_COUNTERCLOCKWISE_90") {
        CHECK(transformRows(template_, StructureMirror::FrontBack, StructureRotation::Counterclockwise90) ==
              "1,5520,7330,7274,8657,6910,25093,5544,25082,141,6922,5526,5486,7330,8657");
    }
}

TEST_CASE("the template queries match the frozen kotlin") {
    SUBCASE("NONE") {
        const Query result = query(StructureRotation::None);
        CHECK(result.box == "10,64,20,12,65,22");
        CHECK(result.jigsaw == "10,64,21/NORTH/UP/false/minecraft:test_name/minecraft:test_pool/minecraft:test_target/"
                               "minecraft:oak_planks/3/7");
        CHECK(result.markers == 1);
        CHECK(result.marker == "12,64,21/chest_east");
    }

    SUBCASE("CLOCKWISE_90") {
        const Query result = query(StructureRotation::Clockwise90);
        CHECK(result.box == "8,64,20,10,65,22");
        CHECK(result.jigsaw == "9,64,20/EAST/UP/false/minecraft:test_name/minecraft:test_pool/minecraft:test_target/"
                               "minecraft:oak_planks/3/7");
        CHECK(result.markers == 1);
        CHECK(result.marker == "9,64,22/chest_east");
    }

    SUBCASE("CLOCKWISE_180") {
        const Query result = query(StructureRotation::Clockwise180);
        CHECK(result.box == "8,64,18,10,65,20");
        CHECK(result.jigsaw == "10,64,19/SOUTH/UP/false/minecraft:test_name/minecraft:test_pool/minecraft:test_target/"
                               "minecraft:oak_planks/3/7");
        CHECK(result.markers == 1);
        CHECK(result.marker == "8,64,19/chest_east");
    }

    SUBCASE("COUNTERCLOCKWISE_90") {
        const Query result = query(StructureRotation::Counterclockwise90);
        CHECK(result.box == "10,64,18,12,65,20");
        CHECK(result.jigsaw == "11,64,20/WEST/UP/false/minecraft:test_name/minecraft:test_pool/minecraft:test_target/"
                               "minecraft:oak_planks/3/7");
        CHECK(result.markers == 1);
        CHECK(result.marker == "11,64,18/chest_east");
    }
}

TEST_CASE("the template placement matches the frozen kotlin") {
    SUBCASE("NONE_NONE") {
        const Placement result = place(StructureMirror::None, StructureRotation::None);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7997708215003342062LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("NONE_CLOCKWISE_90") {
        const Placement result = place(StructureMirror::None, StructureRotation::Clockwise90);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5452013864433454634LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("NONE_CLOCKWISE_180") {
        const Placement result = place(StructureMirror::None, StructureRotation::Clockwise180);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1333519633193866014LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("NONE_COUNTERCLOCKWISE_90") {
        const Placement result = place(StructureMirror::None, StructureRotation::Counterclockwise90);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -712786337524166900LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("LEFT_RIGHT_NONE") {
        const Placement result = place(StructureMirror::LeftRight, StructureRotation::None);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -502499918426698890LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("LEFT_RIGHT_CLOCKWISE_90") {
        const Placement result = place(StructureMirror::LeftRight, StructureRotation::Clockwise90);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6588805142623502190LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("LEFT_RIGHT_CLOCKWISE_180") {
        const Placement result = place(StructureMirror::LeftRight, StructureRotation::Clockwise180);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4900943506505253174LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("LEFT_RIGHT_COUNTERCLOCKWISE_90") {
        const Placement result = place(StructureMirror::LeftRight, StructureRotation::Counterclockwise90);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 167431415336113236LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("FRONT_BACK_NONE") {
        const Placement result = place(StructureMirror::FrontBack, StructureRotation::None);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5094287750135331190LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("FRONT_BACK_CLOCKWISE_90") {
        const Placement result = place(StructureMirror::FrontBack, StructureRotation::Clockwise90);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5854677159728559996LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("FRONT_BACK_CLOCKWISE_180") {
        const Placement result = place(StructureMirror::FrontBack, StructureRotation::Clockwise180);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 3005293928753410350LL);
        CHECK(result.after == -1652463651);
    }

    SUBCASE("FRONT_BACK_COUNTERCLOCKWISE_90") {
        const Placement result = place(StructureMirror::FrontBack, StructureRotation::Counterclockwise90);
        CHECK(result.placed == 14);
        CHECK(result.entities == 1);
        CHECK(result.blockEntities == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4620678956180869458LL);
        CHECK(result.after == -1652463651);
    }
}

TEST_CASE("the terrain-matching and wide-clip placements match the frozen kotlin") {
    SUBCASE("gravity") {
        // Terrain matching, which re-reads the frozen surface for every column.
        const StructureTemplate built = loaded();
        Chunk chunk = groundChunk(0, 0);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(313LL));
        TemplatePlacement placement;
        placement.position = StructurePos{6, 70, 6};
        placement.rotation = StructureRotation::Clockwise90;
        placement.pivot = StructurePos{0, 0, 0};
        placement.clipTo = StructureBoundingBox{0, -64, 0, 15, 319, 15};
        placement.random = &random;
        placement.gravityOffset = -1;
        built.placeInto(chunk, placement);
        const Folded folded = fold(chunk);
        CHECK(folded.placed == 14);
        CHECK(folded.blockEntities == 1);
        CHECK(chunk.takeGeneratedBlockWrites().size() == 0);
        CHECK(folded.checksum == 7960210524894379031LL);
        CHECK(random.nextInt() == 325476402);
    }

    SUBCASE("wideClip") {
        // A clip wider than the chunk, as the fossil has: what falls outside is deferred.
        const StructureTemplate built = loaded();
        Chunk chunk = groundChunk(0, 0);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(727LL));
        TemplatePlacement placement;
        placement.position = StructurePos{14, 65, 14};
        placement.pivot = StructurePos{0, 0, 0};
        placement.clipTo = StructureBoundingBox{-16, -64, -16, 31, 319, 31};
        placement.random = &random;
        built.placeInto(chunk, placement);
        const Folded folded = fold(chunk);
        CHECK(folded.placed == 7);
        CHECK(folded.blockEntities == 1);
        CHECK(chunk.takeGeneratedBlockWrites().size() == 7);
        CHECK(folded.checksum == -6761461911522555127LL);
        CHECK(random.nextInt() == -32861650);
    }
}

TEST_CASE("the decoration placement matches the frozen kotlin") {
    // The decoration entry point, which defers whatever crosses the chunk border.
    const StructureTemplate built = loaded();
    Chunk chunk = groundChunk(0, 0);
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(881LL));
    const bool ok = built.placeDecorationInto(chunk, StructurePos{14, 66, 15}, StructureRotation::Clockwise90, random);
    const Folded folded = fold(chunk);
    CHECK(ok);
    CHECK(folded.placed == 4);
    CHECK(folded.blockEntities == 0);
    CHECK(chunk.takeGeneratedBlockWrites().size() == 10);
    CHECK(folded.checksum == -4351221738467115728LL);
    CHECK(random.nextInt() == -1824420011);

    const StructureTemplate empty;
    Chunk other = groundChunk(0, 0);
    rng::RandomSource otherRandom = rng::RandomSource::worldgen(rng::RandomSource::legacy(1LL));
    CHECK_FALSE(empty.placeDecorationInto(other, StructurePos{0, 64, 0}, StructureRotation::None, otherRandom));
}
