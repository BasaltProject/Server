#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/TagCommand.hpp>
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
    std::vector<std::array<double, 3>> teleports;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.teleportTo = [this](double x, double y, double z, float, float) { teleports.push_back({x, y, z}); };
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
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

TEST_CASE("tag add reports how many actually gained it") {
    Fixture fixture{scratch("tag-command-add")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TagCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tag @a add friendly") == 2);
    CHECK(steve.tags.contains("friendly"));
    CHECK(fixture.source.lastAsText().find("commands.tag.add.success.multiple") != std::string::npos);

    // Everyone already has it, so nothing changed and vanilla refuses rather than reporting zero.
    CHECK(fixture.dispatcher.execute(fixture.source, "tag @a add friendly") == 0);
    CHECK(fixture.source.lastAsText().find("commands.tag.add.failed.multiple") != std::string::npos);
}

TEST_CASE("tag remove takes it back off, and refuses when nobody had it") {
    Fixture fixture{scratch("tag-command-remove")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TagCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tag Steve remove friendly") == 0);
    CHECK(fixture.source.lastAsText().find("commands.tag.remove.failed.single") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "tag Steve add friendly") == 1);
    CHECK(fixture.source.lastAsText().find("commands.tag.add.success.single") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "tag Steve remove friendly") == 1);
    CHECK(steve.tags.empty());
}

TEST_CASE("tag list answers with how many distinct tags the targets carry") {
    Fixture fixture{scratch("tag-command-list")};
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TagCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tag Steve list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.tag.list.single.empty") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "tag Steve add red"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "tag Alex add red"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "tag Alex add blue"));

    // Distinct, so the tag both of them carry is counted once.
    CHECK(fixture.dispatcher.execute(fixture.source, "tag @a list") == 2);
    CHECK(fixture.source.lastAsText().find("commands.tag.list.multiple.success") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("blue, red") != std::string::npos);
}

TEST_CASE("spreadplayers pushes everyone at least the distance apart") {
    Fixture fixture{scratch("spread-players")};
    fixture.add("Steve");
    fixture.add("Alex");
    // A fixed draw, so the run is the same every time: the relaxation is what is under test.
    double turn = 0.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::SpreadPlayersCommand>([&turn] {
        turn += 0.37;
        return turn - std::floor(turn);
    }));

    CHECK(fixture.dispatcher.execute(fixture.source, "spreadplayers 0 0 16 64 false @a") == 2);
    CHECK(fixture.source.lastAsText().find("commands.spreadplayers.success.entities") != std::string::npos);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    REQUIRE(fixture.clients[1]->teleports.size() == 1);
    const auto& first = fixture.clients[0]->teleports[0];
    const auto& second = fixture.clients[1]->teleports[0];
    const double dx = first[0] - second[0];
    const double dz = first[2] - second[2];
    CHECK(std::sqrt((dx * dx) + (dz * dz)) >= 16.0);
}

TEST_CASE("spreadplayers refuses a spread wider than the range it may use") {
    Fixture fixture{scratch("spread-players-range")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::SpreadPlayersCommand>([] { return 0.5; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "spreadplayers 0 0 100 10 false @a") == 0);
    CHECK(fixture.source.lastAsText().find("must not be larger than the maximum range") != std::string::npos);
}

TEST_CASE("spreadplayers under caps the height it lands at") {
    Fixture fixture{scratch("spread-players-under")};
    fixture.add("Steve");
    fixture.players[0]->y = 200.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::SpreadPlayersCommand>([] { return 0.5; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "spreadplayers 0 0 1 8 under 70 false @a") == 1);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    CHECK(fixture.clients[0]->teleports[0][1] <= 70.0);
}
