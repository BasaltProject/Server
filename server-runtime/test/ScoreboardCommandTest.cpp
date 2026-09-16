#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/ScoreboardCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
        // The four go out through the connection rather than as packets, because the two doors
        // draw a board out of different ones.
        namespace play = basalt::wire::protocol::play;
        connection.sendObjective = [this](const std::string&, std::uint8_t,
                                          const std::optional<basalt::wire::text::Component>&) {
            packets.push_back(play::SetObjectivePacket::kId);
        };
        connection.sendDisplayObjective = [this](play::DisplaySlot, const std::string&) {
            packets.push_back(play::SetDisplayObjectivePacket::kId);
        };
        connection.sendScore = [this](const std::string&, const std::string&, std::int32_t) {
            packets.push_back(play::SetScorePacket::kId);
        };
        connection.sendScoreRemoved = [this](const std::string&, const std::optional<std::string>&) {
            packets.push_back(play::ResetScorePacket::kId);
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

namespace play = basalt::wire::protocol::play;

using basalt::server::runtime::scoreboard::Objective;
using basalt::server::runtime::scoreboard::Scoreboard;

TEST_CASE("scoreboard objectives add makes one and refuses a duplicate") {
    Fixture fixture{scratch("scoreboard-objectives-add")};
    Scoreboard scoreboard;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy") == 1);
    REQUIRE(scoreboard.get("kills") != nullptr);
    CHECK(scoreboard.get("kills")->criterion == "dummy");
    CHECK(fixture.clients[0]->sent(play::SetObjectivePacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.objectives.add.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy") == 0);
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.objectives.add.duplicate") != std::string::npos);
}

TEST_CASE("scoreboard objectives list names them, and remove takes one off") {
    Fixture fixture{scratch("scoreboard-objectives-list")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.objectives.list.empty") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add deaths deathCount"));
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives list") == 2);
    CHECK(fixture.source.lastAsText().find("kills, deaths") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives remove kills") == 1);
    CHECK(scoreboard.get("kills") == nullptr);
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives remove kills") == 0);
    CHECK(fixture.source.lastAsText().find("arguments.objective.notFound") != std::string::npos);
}

TEST_CASE("setdisplay puts an objective in a slot and clearing an empty one is refused") {
    Fixture fixture{scratch("scoreboard-setdisplay")};
    Scoreboard scoreboard;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives setdisplay sidebar") == 0);
    CHECK(fixture.source.lastAsText().find("display.alreadyEmpty") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives setdisplay sidebar kills") == 0);
    REQUIRE(scoreboard.displayed(play::DisplaySlot::Sidebar) != nullptr);
    CHECK(*scoreboard.displayed(play::DisplaySlot::Sidebar) == "kills");
    CHECK(fixture.clients[0]->sent(play::SetDisplayObjectivePacket::kId));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives setdisplay sidebar kills") == 0);
    CHECK(fixture.source.lastAsText().find("display.alreadySet") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives setdisplay sidebar") == 0);
    CHECK(scoreboard.displayed(play::DisplaySlot::Sidebar) == nullptr);
}

TEST_CASE("objectives modify moves the display name, render type and number format") {
    Fixture fixture{scratch("scoreboard-modify")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     R"(scoreboard objectives modify kills displayname {"text":"Kills"})") == 0);
    CHECK(flatten(scoreboard.get("kills")->displayName).find("Kills") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives modify kills rendertype hearts") == 0);
    CHECK(scoreboard.get("kills")->renderType == play::ObjectiveRenderType::Hearts);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives modify kills numberformat blank") == 0);
    REQUIRE(scoreboard.get("kills")->numberFormat.has_value());
    CHECK(scoreboard.get("kills")->numberFormat->as<play::NumberFormat::Blank>() != nullptr);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives modify kills numberformat") == 0);
    CHECK_FALSE(scoreboard.get("kills")->numberFormat.has_value());
}

TEST_CASE("players set, add and remove move a score and answer with the total") {
    Fixture fixture{scratch("scoreboard-players-set")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players set #total kills 10") == 10);
    CHECK(scoreboard.score("#total", "kills") == 10);
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players add #total kills 5") == 15);
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players remove #total kills 3") == 12);
    CHECK(scoreboard.score("#total", "kills") == 12);
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.players.remove.success.single") != std::string::npos);
}

TEST_CASE("players get refuses a holder with no score at all") {
    Fixture fixture{scratch("scoreboard-players-get")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players get #total kills") == 0);
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.players.get.null") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #total kills 7"));
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players get #total kills") == 7);
}

TEST_CASE("players reset drops one objective's score or every one of them") {
    Fixture fixture{scratch("scoreboard-players-reset")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add deaths dummy"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #total kills 4"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #total deaths 2"));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players reset #total kills") == 1);
    CHECK_FALSE(scoreboard.score("#total", "kills").has_value());
    CHECK(scoreboard.score("#total", "deaths") == 2);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players reset #total") == 1);
    CHECK_FALSE(scoreboard.score("#total", "deaths").has_value());
}

TEST_CASE("players enable needs a trigger objective and starts the score at zero") {
    Fixture fixture{scratch("scoreboard-players-enable")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add menu trigger"));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players enable Steve kills") == 0);
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.players.enable.invalid") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players enable Steve menu") == 1);
    CHECK(scoreboard.score("Steve", "menu") == 0);
    CHECK(scoreboard.isTriggerEnabled("Steve", "menu"));
}

TEST_CASE("players operation applies the nine, flooring division as vanilla does") {
    Fixture fixture{scratch("scoreboard-players-operation")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add a dummy"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add b dummy"));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #x a -7"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #y b 2"));
    // parity: vanilla floors, so -7 / 2 is -4, not -3.
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players operation #x a /= #y b") == -4);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #x a -7"));
    // parity: `Math.floorMod`, whose answer takes the divisor's sign.
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players operation #x a %= #y b") == 1);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #x a 3"));
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players operation #x a >< #y b") == 2);
    CHECK(scoreboard.score("#y", "b") == 3);
}

TEST_CASE("an operation that would divide by zero is refused") {
    Fixture fixture{scratch("scoreboard-operation-div0")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add a dummy"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #x a 5"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #y a 0"));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players operation #x a /= #y a") == 0);
    CHECK(fixture.source.lastAsText().find("arguments.operation.div0") != std::string::npos);
}

TEST_CASE("players list names every holder, and one holder's scores") {
    Fixture fixture{scratch("scoreboard-players-list")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.players.list.empty") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard players set #total kills 1"));
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players list") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players list #total") == 1);
    CHECK(fixture.source.lastAsText().find("list.entity.success") != std::string::npos);
}

TEST_CASE("a score only reaches clients while its objective is on screen") {
    Fixture fixture{scratch("scoreboard-send")};
    Scoreboard scoreboard;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScoreboardCommand>(scoreboard));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives add kills dummy"));
    fixture.clients[0]->packets.clear();

    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players set #total kills 1") == 1);
    CHECK_FALSE(fixture.clients[0]->sent(play::SetScorePacket::kId));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "scoreboard objectives setdisplay sidebar kills"));
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard players set #total kills 2") == 2);
    CHECK(fixture.clients[0]->sent(play::SetScorePacket::kId));
}
