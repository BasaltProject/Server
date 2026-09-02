#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/SessionCommands.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
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
    basalt::server::runtime::scoreboard::Teams teams;
    std::int32_t idleMinutes = -1;
    std::int32_t restarts = 0;
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
        built.teams = &teams;
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

TEST_CASE("setidletimeout records the value and says which of the two sentences applies") {
    Fixture fixture{scratch("session-commands-idle")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetIdleTimeoutCommand>(
        [&fixture](std::int32_t minutes) { fixture.idleMinutes = minutes; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "setidletimeout 15") == 15);
    CHECK(fixture.idleMinutes == 15);
    CHECK(fixture.source.lastAsText().find("commands.setidletimeout.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "setidletimeout 0") == 0);
    CHECK(fixture.idleMinutes == 0);
    CHECK(fixture.source.lastAsText().find("commands.setidletimeout.success.disabled") != std::string::npos);
}

TEST_CASE("teammsg reaches the sender's own team and nobody else") {
    Fixture fixture{scratch("session-commands-teammsg")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.add("Alex");
    fixture.add("Herobrine");
    basalt::server::runtime::scoreboard::Team* red = fixture.teams.add("red");
    red->members = {"Steve", "Alex"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamMsgCommand>(fixture.teams));

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "teammsg regroup") == 2);

    CHECK(flatten(fixture.clients[0]->messages.at(0)).find("chat.type.team.text") != std::string::npos);
    CHECK(fixture.clients[1]->messages.size() == 1);
    CHECK(fixture.clients[2]->messages.empty());
}

TEST_CASE("teammsg refuses somebody who is on no team") {
    Fixture fixture{scratch("session-commands-teammsg-none")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamMsgCommand>(fixture.teams));

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "teammsg regroup") == 0);
    CHECK(fixture.source.lastAsText().find("commands.teammsg.failed.noteam") != std::string::npos);
}

TEST_CASE("teammsg answers to its alias") {
    Fixture fixture{scratch("session-commands-teammsg-alias")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.teams.add("red")->members = {"Steve"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeamMsgCommand>(fixture.teams));

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "tm regroup") == 1);
}

TEST_CASE("restart warns everyone before it asks for the restart") {
    Fixture fixture{scratch("session-commands-restart")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::RestartCommand>([&fixture] { fixture.restarts++; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "restart") == 1);
    CHECK(fixture.restarts == 1);
    // The warning has to be out before the socket closes, so it is already in the client's hands.
    REQUIRE(fixture.clients[0]->messages.size() == 1);
    CHECK(flatten(fixture.clients[0]->messages[0]).find("restarting") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("1 player(s)") != std::string::npos);
}

TEST_CASE("spectate needs the spectator to be in spectator mode") {
    Fixture fixture{scratch("session-commands-spectate-mode")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::SpectateCommand>());

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "spectate Alex") == 0);
    CHECK(fixture.source.lastAsText().find("commands.spectate.not_spectator") != std::string::npos);

    steve.gamemode = basalt::wire::protocol::play::Gamemode::Spectator;
    CHECK(fixture.dispatcher.execute(asSteve, "spectate Alex") == 1);
    CHECK(fixture.clients[0]->sent(play::SetCameraPacket::kId));
}

TEST_CASE("spectate with no target puts the camera back and refuses spectating yourself") {
    Fixture fixture{scratch("session-commands-spectate-self")};
    PlayerSession& steve = fixture.add("Steve");
    steve.gamemode = basalt::wire::protocol::play::Gamemode::Spectator;
    fixture.dispatcher.registerCommand(std::make_shared<commands::SpectateCommand>());

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "spectate Steve") == 0);
    CHECK(fixture.source.lastAsText().find("commands.spectate.self") != std::string::npos);

    CHECK(fixture.dispatcher.execute(asSteve, "spectate") == 1);
    CHECK(fixture.source.lastAsText().find("commands.spectate.success.stopped") != std::string::npos);
}

TEST_CASE("transfer sends the packet to everyone it names") {
    Fixture fixture{scratch("session-commands-transfer")};
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TransferCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "transfer lobby.example 25566 @a") == 2);
    CHECK(fixture.clients[0]->sent(play::TransferPacket::kId));
    CHECK(fixture.clients[1]->sent(play::TransferPacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.transfer.success.multiple") != std::string::npos);
}

TEST_CASE("transfer moves nobody when the selector matched nobody") {
    Fixture fixture{scratch("session-commands-transfer-none")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TransferCommand>());

    // The selector refuses an empty match before the command sees it, so its own guard against a
    // move of none stays a guard rather than the sentence anyone reads.
    CHECK(fixture.dispatcher.execute(fixture.source, "transfer lobby.example 25566 @a") == 0);
    CHECK(fixture.source.lastAsText().find("commands.transfer.success") == std::string::npos);
}

TEST_CASE("transfer without a port uses the one a client assumes") {
    Fixture fixture{scratch("session-commands-transfer-default")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TransferCommand>());

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "transfer lobby.example") == 1);
    CHECK(fixture.source.lastAsText().find("25565") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("commands.transfer.success.single") != std::string::npos);
}
