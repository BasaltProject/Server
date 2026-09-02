#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::command::CommandDispatcher;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::wire::common::Uuid;
using basalt::wire::text::Component;
namespace commands = basalt::server::runtime::command::commands;

namespace {

/// A source that may do anything and keeps what it was told, which is what a command's answer is.
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

    /// A translatable renders as its key, so its arguments are walked in as well: what a test wants
    /// to see is the sentence the client would build, not the key on its own.
    [[nodiscard]] static std::string flatten(const Component& component) {
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

    mutable std::vector<Component> said;
};

/// One player, with everything its connection was asked to do.
struct Client {
    std::vector<Component> messages;
    std::vector<Component> kicked;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.disconnect = [this](const Component& reason) { kicked.push_back(reason); };
        return connection;
    }
};

struct Fixture {
    PlayerRegistry registry;
    std::vector<std::unique_ptr<PlayerSession>> players;
    std::vector<std::unique_ptr<Client>> clients;
    bool stopped = false;
    CommandEnvironment environment{registry, options()};
    CommandDispatcher dispatcher{environment};
    RecordingSource source;

    [[nodiscard]] CommandEnvironment::Options options() {
        CommandEnvironment::Options built;
        built.worldSeed = 1234;
        built.maxPlayers = 20;
        built.stopServer = [this] { stopped = true; };
        return built;
    }

    PlayerSession& add(const std::string& name) {
        clients.push_back(std::make_unique<Client>());
        players.push_back(std::make_unique<PlayerSession>(Uuid{static_cast<std::uint64_t>(players.size() + 1), 1}, name,
                                                          static_cast<std::int32_t>(players.size() + 1),
                                                          clients.back()->connection()));
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

TEST_CASE("seed answers with the world's own seed") {
    Fixture fixture;
    fixture.dispatcher.registerCommand(std::make_shared<commands::SeedCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "seed") == 1234);
    CHECK(fixture.source.lastAsText().find("1234") != std::string::npos);
}

TEST_CASE("stop tells the server to stop and says so to every operator") {
    Fixture fixture;
    fixture.dispatcher.registerCommand(std::make_shared<commands::StopCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "stop") == 1);
    CHECK(fixture.stopped);
}

TEST_CASE("say announces to everyone, naming whoever ran it") {
    Fixture fixture;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "say hello there") == 1);

    REQUIRE(fixture.clients[0]->messages.size() == 1);
    const std::string said = RecordingSource::flatten(fixture.clients[0]->messages[0]);
    CHECK(said.find("Operator") != std::string::npos);
    CHECK(said.find("hello there") != std::string::npos);
}

TEST_CASE("me is the emote form of the same broadcast, and anyone may use it") {
    Fixture fixture;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::EmoteCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "me waves") == 1);
    CHECK(fixture.clients[0]->messages.size() == 1);
    // parity: `/me` asks for no permission level at all.
    CHECK(commands::EmoteCommand{}.permissionLevel() == 0);
}

TEST_CASE("tellraw sends one component to the players it names") {
    Fixture fixture;
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TellRawCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, R"(tellraw Steve {"text":"hi"})") == 1);

    REQUIRE(fixture.clients[0]->messages.size() == 1);
    CHECK(basalt::wire::text::toPlainText(fixture.clients[0]->messages[0]) == "hi");
    CHECK(fixture.clients[1]->messages.empty());
}

TEST_CASE("tellraw refuses a message that is not a component") {
    Fixture fixture;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TellRawCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tellraw Steve not json") == 0);
    CHECK(fixture.clients[0]->messages.empty());
}

TEST_CASE("kick disconnects the players it names, with a reason") {
    Fixture fixture;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::KickCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "kick Steve") == 1);

    REQUIRE(fixture.clients[0]->kicked.size() == 1);
    // parity: with no reason given it is `multiplayer.disconnect.kicked`.
    const auto* translatable = std::get_if<Component::Translatable>(&fixture.clients[0]->kicked[0].payload());
    REQUIRE(translatable != nullptr);
    CHECK(translatable->key == "multiplayer.disconnect.kicked");

    CHECK(fixture.dispatcher.execute(fixture.source, "kick Steve go away") == 1);
    REQUIRE(fixture.clients[0]->kicked.size() == 2);
    CHECK(basalt::wire::text::toPlainText(fixture.clients[0]->kicked[1]) == "go away");
}

TEST_CASE("kick on nobody is a syntax error rather than a silent success") {
    Fixture fixture;
    fixture.dispatcher.registerCommand(std::make_shared<commands::KickCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "kick Nobody") == 0);
}

TEST_CASE("list counts who is online and names them") {
    Fixture fixture;
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ListPlayersCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "list") == 2);

    const std::string said = fixture.source.lastAsText();
    CHECK(said.find("Steve") != std::string::npos);
    CHECK(said.find("Alex") != std::string::npos);
    CHECK(said.find("20") != std::string::npos);
}

TEST_CASE("list uuids names them with their ids") {
    Fixture fixture;
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ListPlayersCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "list uuids") == 1);
    CHECK(fixture.source.lastAsText().find(steve.uuid().toString()) != std::string::npos);
}

TEST_CASE("version says what this build is and which clients it lets in") {
    Fixture fixture;
    fixture.dispatcher.registerCommand(std::make_shared<commands::VersionCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "version") == 1);

    REQUIRE(fixture.source.said.size() == 2);
    const std::string first = RecordingSource::flatten(fixture.source.said[0]);
    CHECK(first.find("Basalt") != std::string::npos);
    CHECK(first.find("26.2") != std::string::npos);
    CHECK(first.find("776") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("Clients accepted") != std::string::npos);
}

TEST_CASE("version reports a span rather than a list of numbers") {
    Fixture fixture;
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::VersionCommand>([] { return std::set<std::int32_t>{47, 340, 776}; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "version") == 1);

    const std::string said = fixture.source.lastAsText();
    CHECK(said.find(" to ") != std::string::npos);
    CHECK(said.find("3 protocol versions") != std::string::npos);
}

TEST_CASE("version with nothing accepted says so plainly") {
    Fixture fixture;
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::VersionCommand>([] { return std::set<std::int32_t>{}; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "version") == 1);
    CHECK(fixture.source.lastAsText().find("none") != std::string::npos);
}

TEST_CASE("each command asks for the level vanilla asks for") {
    // parity: recorded per command, since it is what a vanilla-facing bridge reports.
    CHECK(commands::SeedCommand{}.permissionLevel() == 2);
    CHECK(commands::SayCommand{}.permissionLevel() == 2);
    CHECK(commands::TellRawCommand{}.permissionLevel() == 2);
    CHECK(commands::KickCommand{}.permissionLevel() == 3);
    CHECK(commands::StopCommand{}.permissionLevel() == 4);
    CHECK(commands::ListPlayersCommand{}.permissionLevel() == 0);
    // Each names the permission `groups.toml` grants by wildcard.
    CHECK(commands::SeedCommand{}.permission() == "basalt.command.seed");
}
