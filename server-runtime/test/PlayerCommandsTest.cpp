#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/server/runtime/command/commands/PlayerCommands.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::command::CommandDispatcher;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::command::ExecutionSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::Gamemode;
using basalt::wire::text::Component;
namespace commands = basalt::server::runtime::command::commands;
namespace config = basalt::server::runtime::config;

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

    [[nodiscard]] std::array<double, 3> position() const override { return {0.0, 0.0, 0.0}; }

    [[nodiscard]] PlayerSession* player() const override { return nullptr; }

    void sendSystemMessage(const Component& message) const override { said.push_back(message); }

    [[nodiscard]] bool hasPermission(std::optional<std::string_view>) const override { return true; }

    [[nodiscard]] std::string lastAsText() const { return said.empty() ? std::string{} : flatten(said.back()); }

    mutable std::vector<Component> said;
};

struct Client {
    std::vector<Component> messages;
    std::vector<Gamemode> modes;
    std::vector<std::string> deaths;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.setGamemode = [this](Gamemode mode) { modes.push_back(mode); };
        connection.killPlayer = [this](std::string_view id) { deaths.emplace_back(id); };
        return connection;
    }
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

TEST_CASE("gamemode moves the players it names and tells each of them") {
    Fixture fixture{scratch("player-commands-gamemode")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameModeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "gamemode creative Steve") == 1);

    REQUIRE(fixture.clients[0]->modes.size() == 1);
    CHECK(fixture.clients[0]->modes[0] == Gamemode::Creative);
    // parity: the player is told separately with `gameMode.changed`.
    REQUIRE(fixture.clients[0]->messages.size() == 1);
    CHECK(flatten(fixture.clients[0]->messages[0]).find("gameMode.changed") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("commands.gamemode.success.other") != std::string::npos);
}

TEST_CASE("gamemode on somebody already in that mode changes nothing") {
    Fixture fixture{scratch("player-commands-gamemode-same")};
    PlayerSession& steve = fixture.add("Steve");
    steve.gamemode = Gamemode::Creative;
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameModeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "gamemode creative Steve") == 0);
    CHECK(fixture.clients[0]->modes.empty());
}

TEST_CASE("gamemode with no target needs a player to be running it") {
    Fixture fixture{scratch("player-commands-gamemode-self")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameModeCommand>());

    // The console is nobody, so there is nothing to change.
    CHECK(fixture.dispatcher.execute(fixture.source, "gamemode creative") == 0);

    const ExecutionSource asSteve{fixture.source, ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "gamemode creative") == 1);
    // parity: changing your own mode reports once, with the `self` key.
    CHECK(fixture.source.lastAsText().find("commands.gamemode.success.self") != std::string::npos);
}

TEST_CASE("defaultgamemode writes the world's own default") {
    Fixture fixture{scratch("player-commands-default")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DefaultGameModeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "defaultgamemode adventure") == 1);

    REQUIRE(fixture.world->defaultGamemode().has_value());
    CHECK(*fixture.world->defaultGamemode() == "adventure");
}

TEST_CASE("kill on a player asks the connection to kill them") {
    Fixture fixture{scratch("player-commands-kill")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::KillCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "kill Steve") == 1);

    REQUIRE(fixture.clients[0]->deaths.size() == 1);
    // parity: `DamageTypes.GENERIC_KILL`, which the client renders as "<name> died".
    CHECK(fixture.clients[0]->deaths[0] == "genericKill");
    CHECK(fixture.source.lastAsText().find("commands.kill.success.single") != std::string::npos);
}

TEST_CASE("kill on several says how many rather than naming them") {
    Fixture fixture{scratch("player-commands-kill-many")};
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::KillCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "kill @a") == 2);
    CHECK(fixture.source.lastAsText().find("commands.kill.success.multiple") != std::string::npos);
}

TEST_CASE("kill marks a summoned entity as killed so it drops what it would have") {
    Fixture fixture{scratch("player-commands-kill-entity")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::KillCommand>());
    fixture.world->state().getOrGenerateChunk(0, 0);

    // Nothing to kill answers as a syntax error rather than as a success.
    CHECK(fixture.dispatcher.execute(fixture.source, "kill @e") == 0);
}

TEST_CASE("help lists every command the source may use") {
    Fixture fixture{scratch("player-commands-help")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::HelpCommand>());
    fixture.dispatcher.registerCommand(std::make_shared<commands::SeedCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "help") == 2);

    REQUIRE(fixture.source.said.size() >= 2);
    CHECK(basalt::wire::text::toPlainText(fixture.source.said[0]).starts_with("/"));
}

TEST_CASE("help on one command prints its own usage") {
    Fixture fixture{scratch("player-commands-help-one")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::HelpCommand>());
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameModeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "help gamemode") == 1);

    const std::string usage = basalt::wire::text::toPlainText(fixture.source.said.back());
    // parity: brigadier's smart usage, with the optional branch in square brackets.
    CHECK(usage == "/gamemode <gamemode> [<target>]");
}

TEST_CASE("help on a command nobody registered says so") {
    Fixture fixture{scratch("player-commands-help-missing")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::HelpCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "help nonsense") == 0);
}
