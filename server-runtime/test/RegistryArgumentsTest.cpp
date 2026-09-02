#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <doctest.h>

#include <string>

using basalt::server::runtime::command::ArgumentValue;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::Parsed;
using basalt::server::runtime::command::StringReader;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::wire::common::Identifier;
using basalt::wire::protocol::play::DisplaySlot;
using basalt::wire::protocol::play::TeamColor;

namespace command = basalt::server::runtime::command;
namespace parserId = basalt::wire::protocol::play::argumentType;
namespace structure = basalt::server::world::structure;

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

} // namespace

TEST_CASE("a registry entry reads as a resource location but declares its own parser") {
    Fixture fixture;
    const Identifier* parsed =
        fixture.parse<Identifier>(command::resourceArgument("minecraft:enchantment"), "sharpness");
    REQUIRE(parsed != nullptr);
    CHECK(parsed->toString() == "minecraft:sharpness");

    const basalt::wire::protocol::play::ArgumentParser wire =
        command::resourceArgument("minecraft:enchantment")->wire();
    CHECK(wire.registryId() == parserId::kResource);
    const auto* properties = wire.as<basalt::wire::protocol::play::ArgumentParser::RegistryProperties>();
    REQUIRE(properties != nullptr);
    CHECK(properties->registry == "minecraft:enchantment");
}

TEST_CASE("the parsers that name no registry carry no properties") {
    CHECK(command::functionArgument()->wire().registryId() == parserId::kFunction);
    CHECK(command::functionArgument()->wire().as<basalt::wire::protocol::play::ArgumentParser::None>() != nullptr);
    CHECK(command::dialogArgument()->wire().registryId() == parserId::kDialog);
}

TEST_CASE("a name the client already keeps reads as a string") {
    Fixture fixture;
    const std::string* objective = fixture.parse<std::string>(command::objectiveArgument(), "\"my score\" rest");
    REQUIRE(objective != nullptr);
    CHECK(*objective == "my score");
    CHECK(command::objectiveArgument()->wire().registryId() == parserId::kObjective);
    CHECK(command::teamArgument()->wire().registryId() == parserId::kTeam);
    CHECK(command::itemSlotArgument()->wire().registryId() == parserId::kItemSlot);
    CHECK(command::objectiveCriteriaArgument()->wire().registryId() == parserId::kObjectiveCriteria);
}

TEST_CASE("an int range is kept as written, open ends included") {
    Fixture fixture;
    for (const std::string form : {"3", "3..", "..7", "3..7"}) {
        const std::string* parsed = fixture.parse<std::string>(command::intRangeArgument(), form + " tail");
        REQUIRE(parsed != nullptr);
        CHECK(*parsed == form);
    }
    CHECK(fixture.parse<std::string>(command::intRangeArgument(), " ") == nullptr);
}

TEST_CASE("a hex colour takes the hash or leaves it") {
    Fixture fixture;
    const std::int32_t* withHash = fixture.parse<std::int32_t>(command::hexColorArgument(), "#ff8000");
    REQUIRE(withHash != nullptr);
    CHECK(*withHash == 0xff8000);

    const std::int32_t* bare = fixture.parse<std::int32_t>(command::hexColorArgument(), "0a0b0c");
    REQUIRE(bare != nullptr);
    CHECK(*bare == 0x0a0b0c);

    CHECK(fixture.parse<std::int32_t>(command::hexColorArgument(), "#zzz") == nullptr);
    // A trailing letter is not a partial read: the whole word has to be hexadecimal.
    CHECK(fixture.parse<std::int32_t>(command::hexColorArgument(), "ff8000g") == nullptr);
}

TEST_CASE("a team colour is one of the sixteen and nothing else") {
    Fixture fixture;
    const TeamColor* parsed = fixture.parse<TeamColor>(command::teamColorArgument(), "dark_purple");
    REQUIRE(parsed != nullptr);
    CHECK(*parsed == TeamColor::DarkPurple);
    CHECK(fixture.parse<TeamColor>(command::teamColorArgument(), "octarine") == nullptr);
}

TEST_CASE("a scoreboard slot is one of the three this build draws") {
    Fixture fixture;
    const DisplaySlot* parsed = fixture.parse<DisplaySlot>(command::scoreboardSlotArgument(), "below_name");
    REQUIRE(parsed != nullptr);
    CHECK(*parsed == DisplaySlot::BelowName);
    // parity gap: vanilla's per-team `sidebar.team.<colour>` slots are refused rather than aliased.
    CHECK(fixture.parse<DisplaySlot>(command::scoreboardSlotArgument(), "sidebar.team.red") == nullptr);
}

TEST_CASE("a template rotation and mirror read by their serialized names") {
    Fixture fixture;
    const auto* rotation =
        fixture.parse<structure::StructureRotation>(command::templateRotationArgument(), "clockwise_90");
    REQUIRE(rotation != nullptr);
    CHECK(*rotation == structure::StructureRotation::Clockwise90);

    const auto* mirror = fixture.parse<structure::StructureMirror>(command::templateMirrorArgument(), "left_right");
    REQUIRE(mirror != nullptr);
    CHECK(*mirror == structure::StructureMirror::LeftRight);

    CHECK(fixture.parse<structure::StructureRotation>(command::templateRotationArgument(), "sideways") == nullptr);
}
