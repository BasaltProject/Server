#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/TeamCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::command::CommandDispatcher;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Uuid;
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
    std::vector<std::string> switched;
    /// Set once the session exists, so a switch can move it the way the real connection does.
    PlayerSession* session = nullptr;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.switchWorld = [this](basalt::server::runtime::world::LoadedWorld& world, double, double, double) {
            switched.push_back(world.name());
            if (session != nullptr) {
                session->worldName = world.name();
            }
            return true;
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

/// A real database in the scratch directory, since `AccessRepository` is not an interface to stub.
[[nodiscard]] std::unique_ptr<basalt::server::storage::PlayerStore> openStore(const std::string& root) {
    std::error_code ignored;
    std::filesystem::create_directories(root, ignored);
    auto opened = basalt::server::storage::PlayerStore::open(root + "/players.db");
    return opened.has_value() ? std::move(*opened) : nullptr;
}

struct Fixture {
    PlayerRegistry registry;
    std::vector<std::unique_ptr<PlayerSession>> players;
    std::vector<std::unique_ptr<Client>> clients;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<basalt::server::storage::PlayerStore> store;
    CommandEnvironment environment;
    CommandDispatcher dispatcher;
    RecordingSource source;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})),
          store(openStore(root)), environment(registry, options()), dispatcher(environment) {}

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
        clients.back()->session = players.back().get();
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

namespace mobEffects = basalt::server::runtime::world::mobEffects;

namespace play = basalt::wire::protocol::play;

namespace play = basalt::wire::protocol::play;

TEST_CASE("team add makes one and refuses a second under the same name") {
    Fixture fixture{scratch("team-add")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));

    CHECK(fixture.dispatcher.execute(fixture.source, "team add red") == 1);
    REQUIRE(teams.get("red") != nullptr);
    CHECK(fixture.clients[0]->sent(play::SetPlayerTeamPacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.team.add.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "team add red") == 0);
    CHECK(fixture.source.lastAsText().find("commands.team.add.duplicate") != std::string::npos);
}

TEST_CASE("team list names the teams, and one team's members") {
    Fixture fixture{scratch("team-list")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));

    CHECK(fixture.dispatcher.execute(fixture.source, "team list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.team.list.teams.empty") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add red"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add blue"));
    CHECK(fixture.dispatcher.execute(fixture.source, "team list") == 2);
    CHECK(fixture.source.lastAsText().find("red, blue") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "team list red") == 0);
    CHECK(fixture.source.lastAsText().find("commands.team.list.members.empty") != std::string::npos);
}

TEST_CASE("team join moves a member, and leaving one team joins the other") {
    Fixture fixture{scratch("team-join")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add red"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add blue"));

    CHECK(fixture.dispatcher.execute(fixture.source, "team join red Steve") == 1);
    REQUIRE(teams.teamOf("Steve") != nullptr);
    CHECK(teams.teamOf("Steve")->name == "red");
    CHECK(fixture.source.lastAsText().find("commands.team.join.success.single") != std::string::npos);

    // parity: a member belongs to one team, so joining another leaves the first.
    CHECK(fixture.dispatcher.execute(fixture.source, "team join blue Steve") == 1);
    CHECK(teams.teamOf("Steve")->name == "blue");
    CHECK(teams.get("red")->members.empty());

    CHECK(fixture.dispatcher.execute(fixture.source, "team leave Steve") == 1);
    CHECK(teams.teamOf("Steve") == nullptr);
    // Nobody left to take off, so the count is zero even though the name was still given.
    CHECK(fixture.dispatcher.execute(fixture.source, "team leave Steve") == 0);
}

TEST_CASE("team join takes an invented name, which is what a fake player is") {
    Fixture fixture{scratch("team-join-fake")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add red"));

    CHECK(fixture.dispatcher.execute(fixture.source, "team join red #total") == 1);
    CHECK(teams.get("red")->members == std::vector<std::string>{"#total"});
}

TEST_CASE("team empty takes everyone off and refuses when nobody is on") {
    Fixture fixture{scratch("team-empty")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add red"));

    CHECK(fixture.dispatcher.execute(fixture.source, "team empty red") == 0);
    CHECK(fixture.source.lastAsText().find("commands.team.empty.unchanged") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team join red Alex"));
    CHECK(fixture.dispatcher.execute(fixture.source, "team empty red") == 1);
    CHECK(teams.get("red")->members.empty());
}

TEST_CASE("team modify moves each option once and refuses a repeat") {
    Fixture fixture{scratch("team-modify")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add red"));
    basalt::server::runtime::scoreboard::Team* team = teams.get("red");
    REQUIRE(team != nullptr);

    CHECK(fixture.dispatcher.execute(fixture.source, "team modify red color dark_red") == 0);
    CHECK(team->color == play::TeamColor::DarkRed);
    CHECK(fixture.dispatcher.execute(fixture.source, "team modify red color dark_red") == 0);
    CHECK(fixture.source.lastAsText().find("commands.team.option.color.unchanged") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "team modify red color reset") == 0);
    CHECK_FALSE(team->color.has_value());

    CHECK(fixture.dispatcher.execute(fixture.source, "team modify red friendlyFire false") == 0);
    CHECK_FALSE(team->friendlyFire);
    CHECK(fixture.dispatcher.execute(fixture.source, "team modify red friendlyFire false") == 0);
    CHECK(fixture.source.lastAsText().find("alreadyDisabled") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "team modify red nametagVisibility never") == 0);
    CHECK(team->nameTagVisibility == play::NameTagVisibility::Never);
    CHECK(fixture.dispatcher.execute(fixture.source, "team modify red collisionRule pushOwnTeam") == 0);
    CHECK(team->collisionRule == play::CollisionRule::PushOwnTeam);
}

TEST_CASE("team modify takes the three pieces of text a name is drawn from") {
    Fixture fixture{scratch("team-modify-text")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add red"));

    CHECK(fixture.dispatcher.execute(fixture.source, R"(team modify red displayName {"text":"The Reds"})") == 0);
    CHECK(flatten(teams.get("red")->displayName).find("The Reds") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, R"(team modify red prefix {"text":"[R] "})") == 0);
    CHECK(flatten(teams.get("red")->prefix).find("[R] ") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, R"(team modify red suffix {"text":" !"})") == 0);
    CHECK(flatten(teams.get("red")->suffix).find(" !") != std::string::npos);
}

TEST_CASE("team remove drops the team and tells every client") {
    Fixture fixture{scratch("team-remove")};
    basalt::server::runtime::scoreboard::Teams teams;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamCommand>(teams));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "team add red"));

    CHECK(fixture.dispatcher.execute(fixture.source, "team remove red") == 0);
    CHECK(teams.get("red") == nullptr);
    CHECK(fixture.source.lastAsText().find("commands.team.remove.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "team remove red") == 0);
    CHECK(fixture.source.lastAsText().find("team.notFound") != std::string::npos);
}
