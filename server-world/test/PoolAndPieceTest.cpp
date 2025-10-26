#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePieceFill.hpp>
#include <basalt/server/world/structure/VillagePoolJson.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::structure;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::int32_t stateOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

[[nodiscard]] Chunk stoneChunk() {
    Chunk chunk{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y <= 40; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 0; y <= 48; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (!registry::blocks::isAir(id) && id != registry::blocks::stone().stateId) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

struct FillRun {
    std::int32_t placed = 0;
    std::size_t entities = 0;
    bool interior = false;
    std::int32_t at = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

/// Every orientation, so the coordinate swap and the state rotation both show.
[[nodiscard]] FillRun runFill(std::optional<StructureDirection> orientation) {
    Chunk chunk = stoneChunk();
    const StructureBoundingBox box{2, 4, 3, 13, 20, 12};
    const StructureBoundingBox clip{0, 0, 0, 15, 48, 15};
    const StructurePieceFill fill{box, orientation, chunk, clip};
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(4242LL));
    fill.generateBox(0, 0, 0, 8, 6, 7, stateOf("oak_planks"), registry::blocks::air().stateId, false);
    fill.generateMaybeBox(random, 0.4F, 1, 7, 1, 7, 9, 6, stateOf("cobblestone"), stateOf("mossy_cobblestone"), true,
                          false);
    fill.generateUpperHalfSphere(0, 10, 0, 8, 14, 7, stateOf("oak_log"), false);
    // A stair carries facing and half, so the orientation's state transform is visible.
    const std::map<std::string, std::string> facing = {{"facing", "east"}};
    fill.place(registry::blocks::stateIdOf(Identifier::vanilla("oak_stairs"), facing).value_or(0), 3, 2, 4);
    fill.maybePlace(random, 0.5F, 4, 3, 5, stateOf("chest"));
    fill.placeEntity(Identifier::vanilla("chest_minecart"), 5, 4, 6, basalt::wire::nbt::NbtCompound{});
    FillRun result;
    const auto [checksum, placed] = fold(chunk);
    result.placed = placed;
    result.entities = chunk.generatedEntities().size();
    result.interior = fill.isInterior(3, 2, 4);
    result.at = fill.blockAt(3, 2, 4);
    result.deferred = chunk.takeGeneratedBlockWrites().size();
    result.checksum = checksum;
    result.after = random.nextInt();
    return result;
}

constexpr std::string_view kPoolJson = R"json(
{
  "fallback": "minecraft:test/fallback",
  "elements": [
    {
      "weight": 2,
      "element": {
        "element_type": "minecraft:legacy_single_pool_element",
        "location": "minecraft:test/one",
        "projection": "rigid",
        "processors": {
          "processors": [
            {
              "processor_type": "minecraft:rule",
              "rules": [
                {
                  "input_predicate": {
                    "predicate_type": "minecraft:random_block_match",
                    "block": "minecraft:cobblestone",
                    "probability": 0.25
                  },
                  "location_predicate": { "predicate_type": "minecraft:always_true" },
                  "output_state": { "Name": "minecraft:mossy_cobblestone" }
                }
              ]
            },
            { "processor_type": "minecraft:block_rot", "integrity": 0.8 },
            {
              "processor_type": "minecraft:capped",
              "limit": 3,
              "delegate": {
                "rules": [
                  {
                    "input_predicate": {
                      "predicate_type": "minecraft:block_match",
                      "block": "minecraft:oak_planks"
                    },
                    "output_state": {
                      "Name": "minecraft:oak_stairs",
                      "Properties": { "facing": "west", "half": "top" }
                    },
                    "block_entity_modifier": {
                      "type": "minecraft:append_loot",
                      "loot_table": "minecraft:chests/village/village_house"
                    }
                  }
                ]
              }
            }
          ]
        }
      }
    },
    {
      "weight": 1,
      "element": {
        "element_type": "minecraft:single_pool_element",
        "location": "minecraft:test/two",
        "projection": "terrain_matching"
      }
    },
    {
      "weight": 1,
      "element": {
        "element_type": "minecraft:feature_pool_element",
        "feature": "minecraft:oak",
        "projection": "rigid"
      }
    },
    { "weight": 1, "element": { "element_type": "minecraft:empty_pool_element", "projection": "rigid" } }
  ]
}
)json";

[[nodiscard]] std::string describe(const StructurePoolElement& element) {
    std::string text;
    switch (element.kind()) {
    case StructurePoolElement::Kind::Empty:
        text = "empty";
        break;
    case StructurePoolElement::Kind::LegacySingle:
        text = "single/" + element.location() + "/" + (element.ignoreAir() ? "true" : "false");
        break;
    case StructurePoolElement::Kind::Feature:
        text = "feature/" + element.location();
        break;
    case StructurePoolElement::Kind::List:
        text = "list/" + std::to_string(element.listSize());
        break;
    }
    text += element.projection() == StructureProjection::Rigid ? "/RIGID" : "/TERRAIN_MATCHING";
    return text + "/" + std::to_string(element.groundLevelDelta());
}

[[nodiscard]] std::string join(const std::vector<std::string>& parts) {
    std::string text;
    for (const std::string& part : parts) {
        if (!text.empty()) {
            text += ',';
        }
        text += part;
    }
    return text;
}

[[nodiscard]] const StructureTemplatePool& parsedPool() {
    static const StructureTemplatePool pool = [] {
        std::expected<StructureTemplatePool, villagePoolJson::ParseError> parsed =
            villagePoolJson::parsePool(kPoolJson, nullptr);
        REQUIRE(parsed.has_value());
        return std::move(*parsed);
    }();
    return pool;
}

[[nodiscard]] std::string rotationName(StructureRotation rotation) {
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
}

void checkFill(const FillRun& result, std::int32_t placed, std::size_t entities, bool interior, std::int32_t at,
               std::size_t deferred, std::int64_t checksum, std::int32_t after) {
    CHECK(result.placed == placed);
    CHECK(result.entities == entities);
    CHECK(result.interior == interior);
    CHECK(result.at == at);
    CHECK(result.deferred == deferred);
    CHECK(result.checksum == checksum);
    CHECK(result.after == after);
}

} // namespace

TEST_CASE("the piece fill writes what the frozen kotlin writes") {
    SUBCASE("fillFree") { checkFill(runFill(std::nullopt), 576, 1, true, 3978, 0, -4585442089900045651LL, 1440546838); }

    SUBCASE("fillNORTH") {
        checkFill(runFill(StructureDirection::North), 576, 1, true, 3978, 0, 8327279442081478337LL, 1440546838);
    }

    SUBCASE("fillSOUTH") {
        checkFill(runFill(StructureDirection::South), 576, 1, true, 3978, 0, 6176489849520537063LL, 1440546838);
    }

    SUBCASE("fillWEST") {
        checkFill(runFill(StructureDirection::West), 576, 1, true, 3938, 0, 5585959790461109351LL, 1440546838);
    }

    SUBCASE("fillEAST") {
        checkFill(runFill(StructureDirection::East), 576, 1, true, 3938, 0, -8590703105063170555LL, 1440546838);
    }
}

TEST_CASE("the pool json parses into what the frozen kotlin parses") {
    const StructureTemplatePool& pool = parsedPool();
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(9001LL));

    SUBCASE("pool") {
        std::vector<std::string> shuffled;
        for (const StructurePoolElement& element : pool.getShuffledTemplates(random)) {
            shuffled.push_back(describe(element));
        }
        std::vector<std::string> drawn;
        for (std::int32_t index = 0; index < 8; ++index) {
            drawn.push_back(describe(pool.getRandomTemplate(random)));
        }
        CHECK(pool.fallback() == "minecraft:test/fallback");
        CHECK(pool.size() == 5);
        CHECK(join(shuffled) ==
              "single/minecraft:test/one/true/RIGID/1,feature/minecraft:oak/RIGID/1,single/minecraft:test/one/true/"
              "RIGID/1,single/minecraft:test/two/false/TERRAIN_MATCHING/1,empty/RIGID/0");
        CHECK(join(drawn) ==
              "single/minecraft:test/one/true/RIGID/1,empty/RIGID/0,single/minecraft:test/one/true/RIGID/1,empty/RIGID/"
              "0,single/minecraft:test/one/true/RIGID/1,single/minecraft:test/two/false/TERRAIN_MATCHING/1,feature/"
              "minecraft:oak/RIGID/1,single/minecraft:test/one/true/RIGID/1");
    }

    SUBCASE("processors") {
        rng::RandomSource own = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(1LL));
        const std::vector<StructurePoolElement> shuffled = pool.getShuffledTemplates(own);
        const StructurePoolElement* single = nullptr;
        for (const StructurePoolElement& element : shuffled) {
            if (element.kind() == StructurePoolElement::Kind::LegacySingle &&
                element.location() == "minecraft:test/one") {
                single = &element;
                break;
            }
        }
        REQUIRE(single != nullptr);
        const StructureProcessorList& processors = single->processors();
        CHECK(processors.rules().size() == 1);
        CHECK(processors.capped().size() == 1);
        REQUIRE(!processors.capped().empty());
        CHECK(processors.capped().front().limit == 3);
        REQUIRE(!processors.rules().empty());
        CHECK(processors.rules().front().outputStateId == 3368);
        REQUIRE(!processors.capped().front().delegate.empty());
        CHECK(processors.capped().front().delegate.front().appendLootTable.value_or("null") ==
              "minecraft:chests/village/village_house");
    }

    SUBCASE("rotations") {
        rng::RandomSource shuffleRandom = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(77LL));
        std::vector<std::string> names;
        for (const StructureRotation rotation : structureRotations::shuffled(shuffleRandom)) {
            names.push_back(rotationName(rotation));
        }
        rng::RandomSource singleRandom = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(77LL));
        CHECK(join(names) == "COUNTERCLOCKWISE_90,CLOCKWISE_180,CLOCKWISE_90,NONE");
        CHECK(rotationName(structureRotations::random(singleRandom)) == "NONE");
    }
}
