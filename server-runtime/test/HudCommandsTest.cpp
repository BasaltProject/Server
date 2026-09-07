#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/HudCommands.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
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
    std::int32_t syncs = 0;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.syncExperience = [this] { syncs++; };
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
        built.access = store == nullptr ? nullptr : &store->access();
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

namespace play = basalt::wire::protocol::play;

TEST_CASE("title sends each of the three lines to everyone it names") {
    Fixture fixture{scratch("hud-commands-title")};
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TitleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "title @a title \"Hello\"") == 2);
    CHECK(fixture.clients[0]->sent(play::kSetTitleTextId));
    CHECK(fixture.clients[1]->sent(play::kSetTitleTextId));
    CHECK(fixture.source.lastAsText().find("commands.title.show.title.multiple") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "title Steve subtitle \"Hi\"") == 1);
    CHECK(fixture.clients[0]->sent(play::kSetSubtitleTextId));
    CHECK(fixture.source.lastAsText().find("commands.title.show.subtitle.single") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "title Steve actionbar \"Hi\"") == 1);
    CHECK(fixture.clients[0]->sent(play::kSetActionBarTextId));
}

TEST_CASE("title clear and reset differ only in whether the timings go back too") {
    Fixture fixture{scratch("hud-commands-title-clear")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TitleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "title Steve clear") == 1);
    CHECK(fixture.source.lastAsText().find("commands.title.cleared.single") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "title Steve reset") == 1);
    CHECK(fixture.source.lastAsText().find("commands.title.reset.single") != std::string::npos);
    CHECK(fixture.clients[0]->sent(play::ClearTitlesPacket::kId));
}

TEST_CASE("title times takes three tick counts") {
    Fixture fixture{scratch("hud-commands-title-times")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TitleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "title Steve times 10 70 20") == 1);
    CHECK(fixture.clients[0]->sent(play::SetTitlesAnimationPacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.title.times.single") != std::string::npos);
}

TEST_CASE("experience add gives points by default and levels when told to") {
    Fixture fixture{scratch("hud-commands-xp-add")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExperienceCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "experience add Steve 100") == 1);
    // 7 + 9 + 11 + 13 + 15 + 17 + 19 is 91, so a hundred points is level 7 with nine to spare.
    CHECK(steve.experience.level() == 7);

    CHECK(fixture.dispatcher.execute(fixture.source, "xp add Steve 3 levels") == 1);
    CHECK(steve.experience.level() == 10);
    CHECK(fixture.source.lastAsText().find("commands.experience.add.levels.success.single") != std::string::npos);
}

TEST_CASE("experience set refuses points worth more than the level they sit in") {
    Fixture fixture{scratch("hud-commands-xp-set")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExperienceCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "experience set Steve 3 points") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "experience set Steve 999 points") == 0);
    CHECK(fixture.source.lastAsText().find("commands.experience.set.points.invalid") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "experience set Steve 5 levels") == 1);
    CHECK(steve.experience.level() == 5);
}

TEST_CASE("experience query answers with the number itself") {
    Fixture fixture{scratch("hud-commands-xp-query")};
    PlayerSession& steve = fixture.add("Steve");
    steve.experience.setLevels(4);
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExperienceCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "experience query Steve levels") == 4);
    CHECK(fixture.source.lastAsText().find("commands.experience.query.levels") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "experience query Steve points") == 0);
}

TEST_CASE("experience pushes the new bar to whoever it moved") {
    Fixture fixture{scratch("hud-commands-xp-sync")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExperienceCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "experience add Steve 10") == 1);
    CHECK(fixture.clients[0]->syncs == 1);
}

TEST_CASE("spawnpoint stores the block it was given, centred") {
    Fixture fixture{scratch("hud-commands-spawnpoint")};
    PlayerSession& steve = fixture.add("Steve");
    REQUIRE(fixture.store != nullptr);
    fixture.dispatcher.registerCommand(std::make_shared<commands::SpawnPointCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "spawnpoint Steve 10 70 -3") == 1);

    const auto stored = fixture.store->access().spawnPointOf(steve.uuid().toString());
    REQUIRE(stored.has_value());
    CHECK(stored->x == doctest::Approx(10.5));
    CHECK(stored->y == doctest::Approx(70.0));
    CHECK(stored->z == doctest::Approx(-2.5));
    CHECK(stored->world == "world");
    CHECK(fixture.source.lastAsText().find("commands.spawnpoint.success.single") != std::string::npos);
}

TEST_CASE("spawnpoint with no position takes where each target stands, floored") {
    Fixture fixture{scratch("hud-commands-spawnpoint-here")};
    PlayerSession& steve = fixture.add("Steve");
    steve.x = -0.5;
    steve.y = 70.9;
    steve.z = 8.2;
    fixture.dispatcher.registerCommand(std::make_shared<commands::SpawnPointCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "spawnpoint Steve") == 1);
    const auto stored = fixture.store->access().spawnPointOf(steve.uuid().toString());
    REQUIRE(stored.has_value());
    CHECK(stored->x == doctest::Approx(-0.5));
    CHECK(stored->y == doctest::Approx(70.0));
    CHECK(stored->z == doctest::Approx(8.5));
}

TEST_CASE("a configured command answers the lines the file gave it") {
    Fixture fixture{scratch("hud-commands-configured")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::ConfiguredCommand>(
        config::CustomCommand{.name = "rules", .message = {"<red>Be nice", "Do not grief"}, .aliases = {"r"}}));

    CHECK(fixture.dispatcher.execute(fixture.source, "rules") == 1);
    REQUIRE(fixture.source.said.size() == 2);
    CHECK(flatten(fixture.source.said[0]).find("Be nice") != std::string::npos);
    CHECK(flatten(fixture.source.said[1]).find("Do not grief") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "r") == 1);
}
