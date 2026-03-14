#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>

using basalt::server::runtime::command::ArgumentValue;
using basalt::server::runtime::command::BlockCoordinates;
using basalt::server::runtime::command::ColumnCoordinates;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::Coordinates;
using basalt::server::runtime::command::ExecutionSource;
using basalt::server::runtime::command::Parsed;
using basalt::server::runtime::command::StringReader;
using basalt::server::runtime::command::WorldCoordinate;
using basalt::server::runtime::player::PlayerRegistry;

namespace command = basalt::server::runtime::command;

namespace {

struct Fixture {
    PlayerRegistry registry;
    CommandEnvironment environment{registry, {}};
    ConsoleCommandSource console;

    template <typename T>
    [[nodiscard]] const T* parse(const std::shared_ptr<const command::ArgumentType>& type, const std::string& input) {
        reader = StringReader{input};
        held = type->parse(reader, console, environment);
        return held.has_value() ? held->as<T>() : nullptr;
    }

    StringReader reader{""};
    Parsed<ArgumentValue> held = std::unexpected{command::CommandSyntaxError{}};
};

/// Which sentence a refusal chose, since the two incomplete keys differ only by dimension.
[[nodiscard]] std::string keyOf(const basalt::wire::text::Component& component) {
    const auto* translatable = std::get_if<basalt::wire::text::Component::Translatable>(&component.payload());
    return translatable == nullptr ? std::string{} : translatable->key;
}

} // namespace

TEST_CASE("a bare integer in a vec3 lands in the middle of its block") {
    Fixture fixture;
    const Coordinates* parsed = fixture.parse<Coordinates>(command::vec3Argument(), "10 64 -3");
    REQUIRE(parsed != nullptr);
    CHECK(parsed->resolve({0.0, 0.0, 0.0}) == std::array<double, 3>{10.5, 64.5, -2.5});
}

TEST_CASE("a decimal point turns the centre correction off") {
    Fixture fixture;
    const Coordinates* parsed = fixture.parse<Coordinates>(command::vec3Argument(), "10.0 64 10");
    REQUIRE(parsed != nullptr);
    CHECK(parsed->resolve({0.0, 0.0, 0.0})[0] == doctest::Approx(10.0));
    CHECK(parsed->resolve({0.0, 0.0, 0.0})[1] == doctest::Approx(64.5));
}

TEST_CASE("a relative coordinate is never centre corrected and reads as an offset") {
    Fixture fixture;
    const Coordinates* parsed = fixture.parse<Coordinates>(command::vec3Argument(), "~ ~3 ~-1");
    REQUIRE(parsed != nullptr);
    CHECK(parsed->x == WorldCoordinate{.relative = true, .value = 0.0});
    CHECK(parsed->resolve({8.0, 70.0, 8.0}) == std::array<double, 3>{8.0, 73.0, 7.0});
}

TEST_CASE("a block position keeps the corner and floors below zero") {
    Fixture fixture;
    const BlockCoordinates* parsed = fixture.parse<BlockCoordinates>(command::blockPosArgument(), "10 64 -3");
    REQUIRE(parsed != nullptr);
    CHECK(parsed->resolve({0.0, 0.0, 0.0}) == std::array<std::int32_t, 3>{10, 64, -3});

    const BlockCoordinates* relative = fixture.parse<BlockCoordinates>(command::blockPosArgument(), "~ ~ ~");
    REQUIRE(relative != nullptr);
    CHECK(relative->resolve({-0.5, 64.0, 0.5}) == std::array<std::int32_t, 3>{-1, 64, 0});
}

TEST_CASE("a two dimensional position leaves the height at zero") {
    Fixture fixture;
    const Coordinates* flat = fixture.parse<Coordinates>(command::vec2Argument(), "10 -3");
    REQUIRE(flat != nullptr);
    CHECK(flat->resolve({0.0, 99.0, 0.0}) == std::array<double, 3>{10.0, 0.0, -3.0});

    const ColumnCoordinates* column = fixture.parse<ColumnCoordinates>(command::columnPosArgument(), "~1 ~-1");
    REQUIRE(column != nullptr);
    CHECK(column->resolve({4.0, 0.0, 4.0}) == std::array<std::int32_t, 2>{5, 3});
}

TEST_CASE("a position that runs out of numbers is refused where it started") {
    Fixture fixture;
    CHECK(fixture.parse<Coordinates>(command::vec3Argument(), "10 64") == nullptr);
    CHECK(fixture.held.error().cursor == 0);
    CHECK(keyOf(fixture.held.error().component) == "argument.pos3d.incomplete");
    CHECK(fixture.parse<ColumnCoordinates>(command::columnPosArgument(), "10") == nullptr);
    CHECK(fixture.held.error().cursor == 0);
    CHECK(keyOf(fixture.held.error().component) == "argument.pos2d.incomplete");
}

TEST_CASE("the separator has to be a space, not merely something that is not a number") {
    Fixture fixture;
    CHECK(fixture.parse<Coordinates>(command::vec3Argument(), "10,64,10") == nullptr);
    CHECK(keyOf(fixture.held.error().component) == "argument.pos3d.incomplete");
}

TEST_CASE("a local triple turns with the source and starts from its anchor") {
    Fixture fixture;
    const Coordinates* parsed = fixture.parse<Coordinates>(command::vec3Argument(), "^1 ^2 ^3");
    REQUIRE(parsed != nullptr);
    CHECK(parsed->local);
    // Facing south, forwards is +z and left is +x.
    const ExecutionSource south{fixture.console, ExecutionSource::Changes{.position = std::array{8.0, 64.0, 8.0},
                                                                          .rotation = std::pair{0.0F, 0.0F}}};
    std::array<double, 3> at = parsed->resolve(south);
    CHECK(at[0] == doctest::Approx(9.0));
    CHECK(at[1] == doctest::Approx(66.0));
    CHECK(at[2] == doctest::Approx(11.0));
    // Facing west, forwards is -x and left is +z; eyes raise the start.
    const ExecutionSource west{south, ExecutionSource::Changes{.rotation = std::pair{90.0F, 0.0F}, .anchorEyes = true}};
    at = parsed->resolve(west);
    CHECK(at[0] == doctest::Approx(5.0));
    CHECK(at[1] == doctest::Approx(67.62));
    CHECK(at[2] == doctest::Approx(9.0));

    const BlockCoordinates* block = fixture.parse<BlockCoordinates>(command::blockPosArgument(), "^ ^ ^-1");
    REQUIRE(block != nullptr);
    CHECK(block->resolve(south) == std::array<std::int32_t, 3>{8, 64, 7});
}

TEST_CASE("local and world coordinates do not mix") {
    Fixture fixture;
    CHECK(fixture.parse<Coordinates>(command::vec3Argument(), "^ ~ ^") == nullptr);
    CHECK(keyOf(fixture.held.error().component) == "argument.pos.mixed");
    CHECK(fixture.parse<Coordinates>(command::vec3Argument(), "~ ^ ~") == nullptr);
    CHECK(keyOf(fixture.held.error().component) == "argument.pos.mixed");
    CHECK(fixture.parse<ColumnCoordinates>(command::columnPosArgument(), "^ ^") == nullptr);
    CHECK(fixture.parse<Coordinates>(command::vec3Argument(), "^ ^") == nullptr);
    CHECK(keyOf(fixture.held.error().component) == "argument.pos3d.incomplete");
}

TEST_CASE("each shape declares the parser the client validates with") {
    CHECK(command::vec3Argument()->wire().registryId() == basalt::wire::protocol::play::argumentType::kVec3);
    CHECK(command::vec2Argument()->wire().registryId() == basalt::wire::protocol::play::argumentType::kVec2);
    CHECK(command::blockPosArgument()->wire().registryId() == basalt::wire::protocol::play::argumentType::kBlockPos);
    CHECK(command::columnPosArgument()->wire().registryId() == basalt::wire::protocol::play::argumentType::kColumnPos);
}
