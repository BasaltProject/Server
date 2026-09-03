#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/TimeCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
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
    std::int32_t trees = 0;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.sendCommandTree = [this] { trees++; };
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

using basalt::server::runtime::world::WorldClock;

TEST_CASE("time set moves the clock and reports the absolute number") {
    Fixture fixture{scratch("time-command-set")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "time set 6000") == 6000);
    CHECK(fixture.world->clock().totalTicks() == 6000);
    CHECK(fixture.source.lastAsText().find("commands.time.set.absolute") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("minecraft:overworld") != std::string::npos);
}

TEST_CASE("a time marker moves to its next occurrence rather than to an absolute number") {
    Fixture fixture{scratch("time-command-marker")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "time set 18000"));
    CHECK(fixture.dispatcher.execute(fixture.source, "time set day") > 0);
    // parity: the marker is taken modulo the day, so it lands on the next dawn, not on this one.
    const std::optional<std::int32_t> noon = WorldClock::markerTicks("noon");
    REQUIRE(noon.has_value());
    CHECK(fixture.world->clock().totalTicks() > 18000);
    CHECK(fixture.world->clock().totalTicks() % WorldClock::kDayLength == *WorldClock::markerTicks("day"));
    CHECK(fixture.source.lastAsText().find("commands.time.set.time_marker") != std::string::npos);
}

TEST_CASE("a marker nobody declared is refused by name") {
    Fixture fixture{scratch("time-command-marker-unknown")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "time set minecraft:teatime") == 0);
    CHECK(fixture.source.lastAsText().find("commands.time.set.time_marker.not_found") != std::string::npos);
}

TEST_CASE("time add takes a negative and clamps at zero rather than running backwards") {
    Fixture fixture{scratch("time-command-add")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "time set 1000"));
    CHECK(fixture.dispatcher.execute(fixture.source, "time add 500") == 1500);
    CHECK(fixture.dispatcher.execute(fixture.source, "time add -5000") == 0);
    CHECK(fixture.world->clock().totalTicks() == 0);
}

TEST_CASE("time pause, resume and rate move the clock's own settings") {
    Fixture fixture{scratch("time-command-rate")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "time pause") == 1);
    CHECK(fixture.world->clock().paused());
    CHECK(fixture.dispatcher.execute(fixture.source, "time resume") == 1);
    CHECK_FALSE(fixture.world->clock().paused());

    CHECK(fixture.dispatcher.execute(fixture.source, "time rate 0.5") == 1);
    CHECK(fixture.world->clock().rate() == doctest::Approx(0.5F));
    // The bounds are the argument's, so a rate of zero never reaches the clock.
    CHECK(fixture.dispatcher.execute(fixture.source, "time rate 0") == 0);
    CHECK(fixture.world->clock().rate() == doctest::Approx(0.5F));
}

TEST_CASE("time query answers with the clock, the game time and the day") {
    Fixture fixture{scratch("time-command-query")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "time set 50000"));
    CHECK(fixture.dispatcher.execute(fixture.source, "time query time") == 50000);
    CHECK(fixture.source.lastAsText().find("commands.time.query.absolute") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "time query day") == 2);
    CHECK(fixture.dispatcher.execute(fixture.source, "time query gametime") == 0);
    CHECK(fixture.source.lastAsText().find("commands.time.query.gametime") != std::string::npos);
}

TEST_CASE("naming a clock has to name the one this world runs on") {
    Fixture fixture{scratch("time-command-of")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "time of minecraft:overworld set 100") == 100);
    CHECK(fixture.dispatcher.execute(fixture.source, "time of minecraft:the_end set 100") == 0);
    CHECK(fixture.source.lastAsText().find("clocks belong to worlds here") != std::string::npos);
    // The bare form's own extras are not repeated under a named clock, as vanilla does not repeat
    // them either.
    CHECK(fixture.dispatcher.execute(fixture.source, "time of minecraft:overworld query gametime") == 0);
}

TEST_CASE("a dimension with no clock refuses by name") {
    Fixture fixture{scratch("time-command-nether")};
    Fixture nether{scratch("time-command-nether-world")};
    nether.world =
        std::make_unique<LoadedWorld>(config::WorldDeclaration{.name = "world",
                                                               .dimension = config::WorldDimension::Nether,
                                                               .type = config::WorldType::Flat,
                                                               .isDefault = true},
                                      config::WorldSettings{}, WorldMetadata{}, scratch("time-command-nether-world"),
                                      basalt::wire::common::Logger{"Test"});
    CommandEnvironment::Options options;
    options.worlds = {{"world", nether.world.get()}};
    const CommandEnvironment environment{nether.registry, std::move(options)};
    CommandDispatcher dispatcher{environment};
    dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    CHECK(dispatcher.execute(nether.source, "time set 100") == 0);
    CHECK(nether.source.lastAsText().find("commands.time.no_default_clock") != std::string::npos);
}

TEST_CASE("a timeline query needs a timeline the jar declared") {
    Fixture fixture{scratch("time-command-timeline")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "time query minecraft:day") == 0);
    CHECK(fixture.source.lastAsText().find("commands.time.query.timeline.unknown") != std::string::npos);
}
