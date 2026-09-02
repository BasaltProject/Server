#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/SaveCommands.hpp>
#include <basalt/server/runtime/command/commands/ScoreCommands.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
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
    basalt::server::runtime::scoreboard::Scoreboard scoreboard;
    std::int32_t saves = 0;
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
        built.scoreboard = &scoreboard;
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

namespace gameRules = basalt::server::runtime::world::gameRules;

TEST_CASE("gamerule reports the rule it was asked about without changing it") {
    Fixture fixture{scratch("score-commands-gamerule-query")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameRuleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "gamerule keep_inventory") == 0);
    CHECK(fixture.source.lastAsText().find("commands.gamerule.query") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("false") != std::string::npos);
    CHECK_FALSE(fixture.world->gameRules().get(gameRules::kKeepInventory));
}

TEST_CASE("gamerule sets a boolean rule and answers 1 for true") {
    Fixture fixture{scratch("score-commands-gamerule-set")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameRuleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "gamerule keep_inventory true") == 1);
    CHECK(fixture.world->gameRules().get(gameRules::kKeepInventory));
    CHECK(fixture.source.lastAsText().find("commands.gamerule.set") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "gamerule keep_inventory false") == 0);
    CHECK_FALSE(fixture.world->gameRules().get(gameRules::kKeepInventory));
}

TEST_CASE("gamerule answers an integer rule with the number itself") {
    Fixture fixture{scratch("score-commands-gamerule-integer")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameRuleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "gamerule max_command_forks 12") == 12);
    CHECK(fixture.world->gameRules().number(gameRules::kMaxCommandForks) == 12);
    // parity: the bounds a rule declares are the argument's own, so an out of range value never lands.
    CHECK(fixture.dispatcher.execute(fixture.source, "gamerule max_block_modifications 0") == 0);
    CHECK(fixture.world->gameRules().number(gameRules::kMaxBlockModifications) ==
          gameRules::kMaxBlockModifications.defaultNumber);
}

TEST_CASE("gamerule answers to the prefixed name as well as the bare one") {
    Fixture fixture{scratch("score-commands-gamerule-prefixed")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::GameRuleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "gamerule minecraft:keep_inventory true") == 1);
    CHECK(fixture.world->gameRules().get(gameRules::kKeepInventory));
}

TEST_CASE("trigger needs an armed objective of the right criterion") {
    Fixture fixture{scratch("score-commands-trigger-unprimed")};
    PlayerSession& steve = fixture.add("Steve");
    REQUIRE(fixture.scoreboard.add({.name = "menu", .criterion = "trigger"}));
    fixture.dispatcher.registerCommand(std::make_shared<commands::TriggerCommand>(fixture.scoreboard));

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "trigger menu") == 0);
    CHECK(fixture.source.lastAsText().find("commands.trigger.failed.unprimed") != std::string::npos);

    fixture.scoreboard.enableTrigger("Steve", "menu");
    CHECK(fixture.dispatcher.execute(asSteve, "trigger menu") == 1);
    CHECK(fixture.scoreboard.score("Steve", "menu") == 1);
    // Running it disarms it, so the next one is refused until it is enabled again.
    CHECK(fixture.dispatcher.execute(asSteve, "trigger menu") == 0);
}

TEST_CASE("trigger refuses an objective that counts something else") {
    Fixture fixture{scratch("score-commands-trigger-invalid")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.scoreboard.add({.name = "deaths", .criterion = "deathCount"});
    fixture.scoreboard.enableTrigger("Steve", "deaths");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TriggerCommand>(fixture.scoreboard));

    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};
    CHECK(fixture.dispatcher.execute(asSteve, "trigger deaths") == 0);
    CHECK(fixture.source.lastAsText().find("commands.trigger.failed.invalid") != std::string::npos);
}

TEST_CASE("trigger add and set move the score by their own amounts") {
    Fixture fixture{scratch("score-commands-trigger-forms")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.scoreboard.add({.name = "menu", .criterion = "trigger"});
    fixture.dispatcher.registerCommand(std::make_shared<commands::TriggerCommand>(fixture.scoreboard));
    const basalt::server::runtime::command::ExecutionSource asSteve{
        fixture.source, basalt::server::runtime::command::ExecutionSource::Changes{.player = &steve}};

    fixture.scoreboard.enableTrigger("Steve", "menu");
    CHECK(fixture.dispatcher.execute(asSteve, "trigger menu add 5") == 5);
    CHECK(fixture.source.lastAsText().find("commands.trigger.add.success") != std::string::npos);

    fixture.scoreboard.enableTrigger("Steve", "menu");
    CHECK(fixture.dispatcher.execute(asSteve, "trigger menu add 5") == 10);

    fixture.scoreboard.enableTrigger("Steve", "menu");
    CHECK(fixture.dispatcher.execute(asSteve, "trigger menu set 2") == 2);
    CHECK(fixture.scoreboard.score("Steve", "menu") == 2);
}

TEST_CASE("save-all writes and says so twice, before and after") {
    Fixture fixture{scratch("score-commands-save-all")};
    commands::SaveControl control{[&fixture] { fixture.saves++; }};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SaveAllCommand>(control));

    CHECK(fixture.dispatcher.execute(fixture.source, "save-all") == 1);
    CHECK(fixture.saves == 1);
    REQUIRE(fixture.source.said.size() == 2);
    CHECK(flatten(fixture.source.said[0]).find("commands.save.saving") != std::string::npos);
    CHECK(flatten(fixture.source.said[1]).find("commands.save.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "save-all flush") == 1);
    CHECK(fixture.saves == 2);
}

TEST_CASE("save-off and save-on each refuse when nothing would change") {
    Fixture fixture{scratch("score-commands-save-toggle")};
    commands::SaveControl control{[&fixture] { fixture.saves++; }};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SaveOffCommand>(control));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SaveOnCommand>(control));

    CHECK(fixture.dispatcher.execute(fixture.source, "save-on") == 0);
    CHECK(fixture.source.lastAsText().find("commands.save.alreadyOn") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "save-off") == 1);
    CHECK_FALSE(control.enabled());
    CHECK(fixture.dispatcher.execute(fixture.source, "save-off") == 0);
    CHECK(fixture.source.lastAsText().find("commands.save.alreadyOff") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "save-on") == 1);
    CHECK(control.enabled());
}
