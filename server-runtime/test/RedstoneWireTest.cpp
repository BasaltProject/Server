#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

using Shape = std::map<std::string, std::string>;

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, stateOf(path));
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::int32_t dot() {
    return stateOf("redstone_wire",
                   {{"north", "none"}, {"south", "none"}, {"east", "none"}, {"west", "none"}, {"power", "0"}});
}

[[nodiscard]] std::int32_t crossed() {
    return stateOf("redstone_wire",
                   {{"north", "side"}, {"south", "side"}, {"east", "side"}, {"west", "side"}, {"power", "0"}});
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() {
        state.getOrGenerateChunk(0, 0);
        // A floor, so a dust has something to sit on and its neighbours do too.
        for (std::int32_t x = 4; x <= 12; ++x) {
            for (std::int32_t z = 4; z <= 12; ++z) {
                REQUIRE(state.setBlockState(x, 63, z, stateOf("stone")));
            }
        }
    }

    void put(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        REQUIRE(state.setBlockState(x, y, z, stateId));
    }

    [[nodiscard]] Shape shapeAt(std::int32_t stateId = 0) {
        return redstoneWire::connectionsFor(state, 8, 64, 8, stateId == 0 ? dot() : stateId);
    }
};

} // namespace

TEST_CASE("only redstone dust is redstone dust") {
    CHECK(redstoneWire::isWire(dot()));
    CHECK(redstoneWire::isWire(crossed()));
    CHECK_FALSE(redstoneWire::isWire(stateOf("stone")));
    CHECK_FALSE(redstoneWire::isWire(registry::blocks::air().stateId));
    CHECK(redstoneWire::block() == Identifier::vanilla("redstone_wire"));
}

TEST_CASE("a dust with nothing around it stays a dot") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());

    const Shape shape = loaded.shapeAt();

    // parity: the one clause that keeps a lone dust from drawing itself as a cross.
    CHECK(shape.at("north") == "none");
    CHECK(shape.at("south") == "none");
    CHECK(shape.at("east") == "none");
    CHECK(shape.at("west") == "none");
}

TEST_CASE("a dust that was a cross and now touches nothing draws itself as a cross again") {
    Loaded loaded;
    loaded.put(8, 64, 8, crossed());

    // parity: only a dust that was already a dot is allowed to stay one.
    const Shape shape = loaded.shapeAt(crossed());

    CHECK(shape.at("north") == "side");
    CHECK(shape.at("south") == "side");
    CHECK(shape.at("east") == "side");
    CHECK(shape.at("west") == "side");
}

TEST_CASE("a dust reaches out to the dust beside it") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());
    loaded.put(9, 64, 8, dot());

    const Shape shape = loaded.shapeAt();

    CHECK(shape.at("east") == "side");
    // parity: with nothing on the other axis it straightens out along this one.
    CHECK(shape.at("west") == "side");
    CHECK(shape.at("north") == "none");
    CHECK(shape.at("south") == "none");
}

TEST_CASE("a line on both axes keeps all four sides and a corner keeps only its two") {
    Loaded cross;
    cross.put(8, 64, 8, dot());
    cross.put(9, 64, 8, dot());
    cross.put(8, 64, 9, dot());
    const Shape corner = cross.shapeAt();
    CHECK(corner.at("east") == "side");
    CHECK(corner.at("south") == "side");
    // parity: with something on each axis, neither is straightened out.
    CHECK(corner.at("west") == "none");
    CHECK(corner.at("north") == "none");
}

TEST_CASE("a dust climbs a block when the way up is clear") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());
    loaded.put(9, 64, 8, stateOf("stone"));
    loaded.put(9, 65, 8, dot());

    const Shape shape = loaded.shapeAt();

    // parity: a sturdy face means the dust runs up the side of it.
    CHECK(shape.at("east") == "up");

    // A roof over the dust shuts the way up, and then it sees nothing that way.
    loaded.put(8, 65, 8, stateOf("stone"));
    const Shape roofed = loaded.shapeAt();
    CHECK(roofed.at("east") == "none");
}

TEST_CASE("a dust runs down onto the dust below a step") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());
    // Nothing beside it, and a dust one lower.
    loaded.put(9, 63, 8, dot());

    const Shape shape = loaded.shapeAt();

    CHECK(shape.at("east") == "side");
}

TEST_CASE("a block that carries redstone stops the dust from looking any further") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());
    loaded.put(9, 64, 8, stateOf("stone"));
    // A dust below the stone, which a solid block hides.
    loaded.put(9, 63, 8, dot());

    const Shape shape = loaded.shapeAt();

    CHECK(shape.at("east") == "none");
}

TEST_CASE("a repeater is joined end on and not from the side") {
    Loaded endOn;
    endOn.put(8, 64, 8, dot());
    endOn.put(9, 64, 8,
              stateOf("repeater", {{"facing", "east"}, {"delay", "1"}, {"locked", "false"}, {"powered", "false"}}));
    CHECK(endOn.shapeAt().at("east") == "side");

    Loaded sideways;
    sideways.put(8, 64, 8, dot());
    sideways.put(9, 64, 8,
                 stateOf("repeater", {{"facing", "north"}, {"delay", "1"}, {"locked", "false"}, {"powered", "false"}}));
    CHECK(sideways.shapeAt().at("east") == "none");

    // parity: both ends count, so a repeater pointing back at the dust joins it too.
    Loaded back;
    back.put(8, 64, 8, dot());
    back.put(9, 64, 8,
             stateOf("repeater", {{"facing", "west"}, {"delay", "1"}, {"locked", "false"}, {"powered", "false"}}));
    CHECK(back.shapeAt().at("east") == "side");
}

TEST_CASE("an observer is joined at its face and nowhere else") {
    Loaded facing;
    facing.put(8, 64, 8, dot());
    facing.put(9, 64, 8, stateOf("observer", {{"facing", "east"}, {"powered", "false"}}));
    CHECK(facing.shapeAt().at("east") == "side");

    Loaded backwards;
    backwards.put(8, 64, 8, dot());
    backwards.put(9, 64, 8, stateOf("observer", {{"facing", "west"}, {"powered", "false"}}));
    CHECK(backwards.shapeAt().at("east") == "none");
}

TEST_CASE("anything else that gives off a signal is joined, and anything that does not is not") {
    Loaded lever;
    lever.put(8, 64, 8, dot());
    lever.put(9, 64, 8, stateOf("lever", {{"face", "floor"}, {"facing", "north"}, {"powered", "false"}}));
    CHECK(lever.shapeAt().at("east") == "side");

    Loaded torch;
    torch.put(8, 64, 8, dot());
    torch.put(9, 64, 8, stateOf("redstone_torch", {{"lit", "true"}}));
    CHECK(torch.shapeAt().at("east") == "side");

    Loaded plain;
    plain.put(8, 64, 8, dot());
    plain.put(9, 64, 8, stateOf("torch"));
    CHECK(plain.shapeAt().at("east") == "none");
}

TEST_CASE("a dust climbs over a trapdoor, which nothing else its shape would let it climb") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());
    loaded.put(9, 64, 8,
               stateOf("oak_trapdoor", {{"facing", "north"},
                                        {"half", "bottom"},
                                        {"open", "false"},
                                        {"powered", "false"},
                                        {"waterlogged", "false"}}));
    loaded.put(9, 65, 8, dot());

    const Shape shape = loaded.shapeAt();

    // parity: the trapdoor is the named exception, and its face is not sturdy, so the dust goes
    // along its side rather than up it.
    CHECK(shape.at("east") == "side");
}

TEST_CASE("reshaping writes the new state and answers it, and only when it moved") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());
    loaded.put(9, 64, 8, dot());

    const std::optional<std::int32_t> turned = redstoneWire::reshape(loaded.state, 8, 64, 8, dot());

    REQUIRE(turned.has_value());
    CHECK(loaded.state.blockState(8, 64, 8) == *turned);
    const std::map<std::string, std::string> shown = registry::blockStateDefinitions::propertiesOf(*turned);
    CHECK(shown.at("east") == "side");
    CHECK(shown.at("west") == "side");

    // Asked again it is already right, so nothing moves.
    CHECK_FALSE(redstoneWire::reshape(loaded.state, 8, 64, 8, *turned).has_value());
    // And a block that is not dust is not reshaped at all.
    CHECK_FALSE(redstoneWire::reshape(loaded.state, 8, 64, 8, stateOf("stone")).has_value());
}

TEST_CASE("a dust at the edge of the loaded world sees nothing beyond it") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());

    const Shape shape = redstoneWire::connectionsFor(loaded.state, 500, 64, 500, dot());

    CHECK(shape.at("east") == "none");
    CHECK(shape.at("north") == "none");
}

TEST_CASE("a dust does not climb over something it could not sit on") {
    Loaded loaded;
    loaded.put(8, 64, 8, dot());
    // A torch holds nothing up, so the dust above it is out of reach even with the way up clear.
    loaded.put(9, 64, 8, stateOf("torch"));
    loaded.put(9, 65, 8, dot());

    const Shape shape = loaded.shapeAt();

    CHECK(shape.at("east") == "none");

    // The same dust over a block it could sit on is reached.
    Loaded solid;
    solid.put(8, 64, 8, dot());
    solid.put(9, 64, 8, stateOf("stone"));
    solid.put(9, 65, 8, dot());
    CHECK(solid.shapeAt().at("east") == "up");
}
