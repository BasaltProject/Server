#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/WorldCommands.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::command::CommandDispatcher;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::server::runtime::world::WorldWeather;
using basalt::wire::common::Uuid;
using basalt::wire::text::Component;
namespace commands = basalt::server::runtime::command::commands;
namespace config = basalt::server::runtime::config;
namespace play = basalt::wire::protocol::play;

namespace {

[[nodiscard]] std::string flatten(const Component& component) {
    std::string text = basalt::wire::text::toPlainText(component);
    if (const auto* translatable = std::get_if<Component::Translatable>(&component.payload());
        translatable != nullptr) {
        text += translatable->key;
        for (const Component& argument : translatable->args) {
            text += " " + flatten(argument);
        }
    }
    for (const Component& child : component.children()) {
        text += flatten(child);
    }
    return text;
}

class RecordingSource final : public CommandSource {
public:
    [[nodiscard]] std::string name() const override { return "Operator"; }

    [[nodiscard]] Component displayName() const override { return Component::text("Operator"); }

    [[nodiscard]] std::int32_t permissionLevel() const override { return 4; }

    [[nodiscard]] std::array<double, 3> position() const override { return at; }

    [[nodiscard]] PlayerSession* player() const override { return nullptr; }

    void sendSystemMessage(const Component& message) const override { said.push_back(message); }

    [[nodiscard]] bool hasPermission(std::optional<std::string_view>) const override { return true; }

    [[nodiscard]] std::string lastAsText() const { return said.empty() ? std::string{} : flatten(said.back()); }

    std::array<double, 3> at{0.0, 0.0, 0.0};
    mutable std::vector<Component> said;
};

/// One connection that keeps whatever was sent to it, packets included.
struct Client {
    std::vector<Component> messages;
    std::vector<std::int32_t> packets;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        // The command asks the connection where the spawn went rather than writing the packet
        // itself, because the two doors write different ones.
        connection.sendSpawnPosition = [this](const basalt::server::runtime::world::LoadedWorld&, std::int32_t,
                                              std::int32_t, std::int32_t, float) {
            packets.push_back(basalt::wire::protocol::play::SpawnPositionPacket::kId);
        };
        return connection;
    }

    [[nodiscard]] bool sent(std::int32_t id) const { return std::ranges::find(packets, id) != packets.end(); }
};

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

struct Fixture {
    PlayerRegistry registry;
    std::vector<std::unique_ptr<PlayerSession>> players;
    std::vector<std::unique_ptr<Client>> clients;
    std::unique_ptr<LoadedWorld> world;
    RandomSource random = RandomSource::legacy(42);
    CommandEnvironment environment;
    CommandDispatcher dispatcher;
    RecordingSource source;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})),
          environment(registry, options()), dispatcher(environment) {}

    [[nodiscard]] CommandEnvironment::Options options() {
        CommandEnvironment::Options built;
        built.worlds = {{"world", world.get()}};
        built.random = &random;
        return built;
    }

    PlayerSession& add(const std::string& name) {
        clients.push_back(std::make_unique<Client>());
        players.push_back(std::make_unique<PlayerSession>(Uuid{static_cast<std::uint64_t>(players.size() + 1), 1}, name,
                                                          static_cast<std::int32_t>(players.size() + 1),
                                                          clients.back()->connection()));
        players.back()->worldName = "world";
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

TEST_CASE("msg tells each recipient and reports once per recipient to the sender") {
    Fixture fixture{scratch("world-commands-msg")};
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::MsgCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "msg @a hello there") == 2);

    REQUIRE(fixture.clients[0]->messages.size() == 1);
    CHECK(flatten(fixture.clients[0]->messages[0]).find("commands.message.display.incoming") != std::string::npos);
    CHECK(flatten(fixture.clients[0]->messages[0]).find("hello there") != std::string::npos);
    CHECK(fixture.source.said.size() == 2);
    CHECK(fixture.source.lastAsText().find("commands.message.display.outgoing") != std::string::npos);
}

TEST_CASE("msg answers to its two aliases as well as its name") {
    Fixture fixture{scratch("world-commands-msg-aliases")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::MsgCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tell Steve hi") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "w Steve hi") == 1);
}

TEST_CASE("weather rain turns the rain on for the duration it was given") {
    Fixture fixture{scratch("world-commands-weather")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WeatherCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "weather rain 300") == 300);

    CHECK(fixture.world->weather().raining);
    CHECK_FALSE(fixture.world->weather().thundering);
    CHECK(fixture.world->weather().rainTime == 300);
    CHECK(fixture.world->weather().clearWeatherTime == 0);
    CHECK(fixture.source.lastAsText().find("commands.weather.set.rain") != std::string::npos);
}

TEST_CASE("weather thunder rains as well, since a storm is rain with thunder on top") {
    Fixture fixture{scratch("world-commands-weather-thunder")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WeatherCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "weather thunder 100") == 100);
    CHECK(fixture.world->weather().raining);
    CHECK(fixture.world->weather().thundering);
}

TEST_CASE("weather clear sets the clear countdown rather than a weather one") {
    Fixture fixture{scratch("world-commands-weather-clear")};
    fixture.world->weather().set(0, 500, true, true);
    fixture.dispatcher.registerCommand(std::make_shared<commands::WeatherCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "weather clear 240") == 240);
    CHECK_FALSE(fixture.world->weather().raining);
    CHECK_FALSE(fixture.world->weather().thundering);
    CHECK(fixture.world->weather().clearWeatherTime == 240);
    CHECK(fixture.world->weather().rainTime == 0);
}

TEST_CASE("a weather with no duration draws one from its own range and answers -1") {
    Fixture fixture{scratch("world-commands-weather-random")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WeatherCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "weather thunder") == -1);
    // parity: `THUNDER_DURATION`, which is a narrower range than rain's and never overlaps it.
    CHECK(fixture.world->weather().rainTime >= WorldWeather::kThunderDuration.first);
    CHECK(fixture.world->weather().rainTime <= WorldWeather::kThunderDuration.second);

    Fixture rain{scratch("world-commands-weather-random-rain")};
    rain.dispatcher.registerCommand(std::make_shared<commands::WeatherCommand>());
    CHECK(rain.dispatcher.execute(rain.source, "weather rain") == -1);
    CHECK(rain.world->weather().rainTime >= WorldWeather::kRainDuration.first);

    Fixture clear{scratch("world-commands-weather-random-clear")};
    clear.dispatcher.registerCommand(std::make_shared<commands::WeatherCommand>());
    CHECK(clear.dispatcher.execute(clear.source, "weather clear") == -1);
    CHECK(clear.world->weather().clearWeatherTime >= WorldWeather::kRainDelay.first);
}

TEST_CASE("difficulty sets the world's own and pushes it to whoever is in it") {
    Fixture fixture{scratch("world-commands-difficulty")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::DifficultyCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "difficulty hard") == 3);
    CHECK(fixture.world->difficulty() == "hard");
    CHECK(fixture.clients[0]->sent(play::ChangeDifficultyPacket::kId));
    CHECK(fixture.source.lastAsText().find("options.difficulty.hard") != std::string::npos);
}

TEST_CASE("difficulty refuses the one the world is already on") {
    Fixture fixture{scratch("world-commands-difficulty-same")};
    fixture.add("Steve");
    fixture.world->setDifficulty("easy");
    fixture.dispatcher.registerCommand(std::make_shared<commands::DifficultyCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "difficulty easy") == 0);
    CHECK(fixture.source.lastAsText().find("commands.difficulty.failure") != std::string::npos);
    CHECK(fixture.clients[0]->packets.empty());
}

TEST_CASE("difficulty with no argument answers with the one in force") {
    Fixture fixture{scratch("world-commands-difficulty-query")};
    fixture.world->setDifficulty("normal");
    fixture.dispatcher.registerCommand(std::make_shared<commands::DifficultyCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "difficulty") == 2);
    CHECK(fixture.source.lastAsText().find("commands.difficulty.query") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("options.difficulty.normal") != std::string::npos);
}

TEST_CASE("setworldspawn moves the spawn to the block it was given") {
    Fixture fixture{scratch("world-commands-spawn")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetWorldSpawnCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "setworldspawn 10 70 -3") == 1);

    // parity: a block position is the block's corner, never centre corrected.
    CHECK(fixture.world->spawn() == std::array<double, 3>{10.5, 70.0, -2.5});
    CHECK(fixture.clients[0]->sent(play::SpawnPositionPacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.setworldspawn.success") != std::string::npos);
}

TEST_CASE("setworldspawn with no position uses where the runner stands, floored") {
    Fixture fixture{scratch("world-commands-spawn-here")};
    fixture.source.at = {-0.5, 70.9, 8.2};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetWorldSpawnCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "setworldspawn") == 1);
    CHECK(fixture.world->spawn() == std::array<double, 3>{-0.5, 70.0, 8.5});
}

TEST_CASE("setworldspawn takes the yaw of a rotation and ignores its pitch") {
    Fixture fixture{scratch("world-commands-spawn-rotation")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetWorldSpawnCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "setworldspawn 0 64 0 90 45") == 1);
    CHECK(fixture.world->spawnYaw() == doctest::Approx(90.0F));
}
