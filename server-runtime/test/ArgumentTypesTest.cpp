#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

using basalt::server::runtime::command::ArgumentType;
using basalt::server::runtime::command::ArgumentValue;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::Parsed;
using basalt::server::runtime::command::StringReader;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::world::LoadedWorld;
using basalt::wire::protocol::play::ArgumentParser;
using basalt::wire::protocol::play::Gamemode;
namespace argumentType = basalt::wire::protocol::play::argumentType;
namespace command = basalt::server::runtime::command;

namespace {

struct Fixture {
    PlayerRegistry registry;
    CommandEnvironment environment{registry, {}};
    ConsoleCommandSource console;

    template <typename T>
    [[nodiscard]] std::optional<T> parse(const ArgumentType& type, const std::string& input) {
        StringReader reader{input};
        const Parsed<ArgumentValue> value = type.parse(reader, console, environment);
        if (!value.has_value()) {
            return std::nullopt;
        }
        const T* held = value->as<T>();
        return held == nullptr ? std::nullopt : std::optional{*held};
    }

    [[nodiscard]] bool refuses(const ArgumentType& type, const std::string& input) {
        StringReader reader{input};
        return !type.parse(reader, console, environment).has_value();
    }
};

} // namespace

TEST_CASE("a word is one word and a greedy string is the rest of the line") {
    Fixture fixture;

    CHECK(fixture.parse<std::string>(*command::wordArgument(), "Steve rest") == "Steve");
    CHECK(fixture.parse<std::string>(*command::greedyStringArgument(), "the rest of it") == "the rest of it");
    CHECK(fixture.parse<std::string>(*command::messageArgument(), "hello there") == "hello there");
}

TEST_CASE("a bool takes exactly true and false") {
    Fixture fixture;

    CHECK(fixture.parse<bool>(*command::boolArgument(), "true") == true);
    CHECK(fixture.parse<bool>(*command::boolArgument(), "false") == false);
    CHECK(fixture.refuses(*command::boolArgument(), "yes"));
    CHECK(fixture.refuses(*command::boolArgument(), "TRUE"));
    CHECK(fixture.refuses(*command::boolArgument(), ""));
}

TEST_CASE("a bounded number refuses what is outside its range and keeps the cursor where it was") {
    Fixture fixture;
    const std::shared_ptr<const ArgumentType> count = command::integerArgument(1, 64);

    CHECK(fixture.parse<std::int32_t>(*count, "32") == 32);
    CHECK(fixture.parse<std::int32_t>(*count, "1") == 1);
    CHECK(fixture.parse<std::int32_t>(*count, "64") == 64);
    CHECK(fixture.refuses(*count, "0"));
    CHECK(fixture.refuses(*count, "65"));
    CHECK(fixture.refuses(*count, "notanumber"));

    StringReader reader{"0"};
    static_cast<void>(count->parse(reader, fixture.console, fixture.environment));
    // parity: brigadier rewinds to the start of the number so the marker points at it.
    CHECK(reader.cursor() == 0);
}

TEST_CASE("the bounds a number declares are what the client is told") {
    const std::shared_ptr<const ArgumentType> count = command::integerArgument(1, 64);

    const ArgumentParser parser = count->wire();

    CHECK(parser.registryId() == argumentType::kBrigadierInteger);
    const auto* bounds = parser.as<ArgumentParser::Bounds<std::int32_t>>();
    REQUIRE(bounds != nullptr);
    CHECK(bounds->min == 1);
    CHECK(bounds->max == 64);
}

TEST_CASE("a double and a float are bounded the same way") {
    Fixture fixture;

    CHECK(fixture.parse<double>(*command::doubleArgument(0.0, 1.0), "0.5") == doctest::Approx(0.5));
    CHECK(fixture.refuses(*command::doubleArgument(0.0, 1.0), "2.0"));
    CHECK(fixture.parse<float>(*command::floatArgument(-1.0F, 1.0F), "-0.25") == doctest::Approx(-0.25F));
    CHECK(fixture.refuses(*command::floatArgument(-1.0F, 1.0F), "-2"));
}

TEST_CASE("an angle wraps rather than being refused") {
    Fixture fixture;

    CHECK(fixture.parse<float>(*command::angleArgument(), "90") == doctest::Approx(90.0F));
    // parity: `AngleArgument` wraps into -180..180, so 540 is 180 which is -180.
    CHECK(fixture.parse<float>(*command::angleArgument(), "540") == doctest::Approx(-180.0F));
    CHECK(fixture.parse<float>(*command::angleArgument(), "-90") == doctest::Approx(-90.0F));
    CHECK(fixture.parse<float>(*command::angleArgument(), "~") == doctest::Approx(0.0F));
    CHECK(fixture.parse<float>(*command::angleArgument(), "~45") == doctest::Approx(45.0F));
    CHECK(fixture.refuses(*command::angleArgument(), ""));
}

TEST_CASE("a rotation is two angles and refuses half a pair") {
    Fixture fixture;

    const std::optional<std::pair<float, float>> both =
        fixture.parse<std::pair<float, float>>(*command::rotationArgument(), "90 -20");
    REQUIRE(both.has_value());
    CHECK(both->first == doctest::Approx(90.0F));
    CHECK(both->second == doctest::Approx(-20.0F));

    CHECK(fixture.refuses(*command::rotationArgument(), "90"));
}

TEST_CASE("a time is ticks unless it names a unit") {
    Fixture fixture;

    CHECK(fixture.parse<std::int32_t>(*command::timeArgument(), "40") == 40);
    CHECK(fixture.parse<std::int32_t>(*command::timeArgument(), "40t") == 40);
    CHECK(fixture.parse<std::int32_t>(*command::timeArgument(), "10s") == 200);
    CHECK(fixture.parse<std::int32_t>(*command::timeArgument(), "2d") == 48000);
    // parity: the product is floored rather than rounded.
    CHECK(fixture.parse<std::int32_t>(*command::timeArgument(), "0.5s") == 10);
    CHECK(fixture.parse<std::int32_t>(*command::timeArgument(), "0.04s") == 0);
}

TEST_CASE("a time below what the command allows is refused") {
    Fixture fixture;
    const std::shared_ptr<const ArgumentType> atLeastOne = command::timeArgument(1);

    CHECK(fixture.parse<std::int32_t>(*atLeastOne, "1") == 1);
    CHECK(fixture.refuses(*atLeastOne, "0"));

    const ArgumentParser parser = atLeastOne->wire();
    const auto* properties = parser.as<ArgumentParser::TimeProperties>();
    REQUIRE(properties != nullptr);
    CHECK(properties->min == 1);
}

TEST_CASE("a game mode takes the four names vanilla writes and nothing else") {
    Fixture fixture;

    CHECK(fixture.parse<Gamemode>(*command::gameModeArgument(), "survival") == Gamemode::Survival);
    CHECK(fixture.parse<Gamemode>(*command::gameModeArgument(), "CREATIVE") == Gamemode::Creative);
    CHECK(fixture.parse<Gamemode>(*command::gameModeArgument(), "adventure") == Gamemode::Adventure);
    CHECK(fixture.parse<Gamemode>(*command::gameModeArgument(), "spectator") == Gamemode::Spectator);
    CHECK(fixture.refuses(*command::gameModeArgument(), "hardcore"));
    CHECK(fixture.refuses(*command::gameModeArgument(), "0"));
}

TEST_CASE("a profile name is carried through whether or not anybody has used it") {
    Fixture fixture;

    CHECK(fixture.parse<std::string>(*command::gameProfileArgument(), "Steve") == "Steve");
    // parity: vanilla accepts a name nobody has ever played under, which is how `/op` works offline.
    CHECK(fixture.parse<std::string>(*command::gameProfileArgument(), "NobodyAtAll") == "NobodyAtAll");
}

TEST_CASE("a world is found by name and refused when it is not loaded") {
    const std::filesystem::path root = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / "argument-types-world";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    basalt::server::runtime::world::WorldMetadata metadata;
    LoadedWorld world{basalt::server::runtime::config::WorldDeclaration{
                          .name = "world", .type = basalt::server::runtime::config::WorldType::Flat, .isDefault = true},
                      basalt::server::runtime::config::WorldSettings{}, metadata, root.string(),
                      basalt::wire::common::Logger{"Test"}};
    PlayerRegistry registry;
    CommandEnvironment::Options options;
    options.worlds = {{"world", &world}};
    const CommandEnvironment environment{registry, std::move(options)};
    const ConsoleCommandSource console;

    StringReader found{"world"};
    const Parsed<ArgumentValue> parsed = command::worldArgument()->parse(found, console, environment);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->as<LoadedWorld*>() != nullptr);
    CHECK(*parsed->as<LoadedWorld*>() == &world);

    StringReader byKey{world.levelKey()};
    const Parsed<ArgumentValue> keyed = command::worldArgument()->parse(byKey, console, environment);
    REQUIRE(keyed.has_value());
    CHECK(*keyed->as<LoadedWorld*>() == &world);
    CHECK(byKey.cursor() == static_cast<std::int32_t>(world.levelKey().size()));

    StringReader missing{"nowhere"};
    CHECK_FALSE(command::worldArgument()->parse(missing, console, environment).has_value());
    CHECK(missing.cursor() == 0);
}

TEST_CASE("a world argument tells the client to ask, since the client has no such list") {
    CHECK(command::worldArgument()->wire().registryId() == argumentType::kDimension);
    REQUIRE(command::worldArgument()->clientSuggestions().has_value());
    CHECK(*command::worldArgument()->clientSuggestions() == command::suggestionProviders::kAskServer);
    CHECK(static_cast<bool>(command::worldArgument()->suggestions()));
}

TEST_CASE("the simple types name the parser the client should use") {
    CHECK(command::wordArgument()->wire().registryId() == argumentType::kBrigadierString);
    CHECK(command::boolArgument()->wire().registryId() == argumentType::kBrigadierBool);
    CHECK(command::angleArgument()->wire().registryId() == argumentType::kAngle);
    CHECK(command::rotationArgument()->wire().registryId() == argumentType::kRotation);
    CHECK(command::messageArgument()->wire().registryId() == argumentType::kMessage);
    CHECK(command::gameProfileArgument()->wire().registryId() == argumentType::kGameProfile);
    CHECK(command::gameModeArgument()->wire().registryId() == argumentType::kGamemode);

    const ArgumentParser parser = command::greedyStringArgument()->wire();
    const auto* greedy = parser.as<ArgumentParser::StringProperties>();
    REQUIRE(greedy != nullptr);
    CHECK(greedy->kind == ArgumentParser::StringKind::GreedyPhrase);
}
