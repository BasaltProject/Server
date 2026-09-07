#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/ProfilingCommands.hpp>
#include <basalt/server/runtime/command/commands/TimingsCommand.hpp>
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

/// Every source the report calls, so writing one does not depend on a server being up.
[[nodiscard]] basalt::server::runtime::timings::ReportSources reportSources() {
    basalt::server::runtime::timings::ReportSources sources;
    sources.versionName = "test";
    sources.uptimeSeconds = [] { return std::int64_t{0}; };
    sources.onlinePlayers = [] { return 0; };
    sources.maxPlayers = [] { return 0; };
    sources.loadedChunks = [] { return 0; };
    sources.regionCacheBytes = [] { return std::int64_t{0}; };
    sources.chunkBytes = [] { return std::int64_t{0}; };
    sources.chunkBlockEntities = [] { return 0; };
    sources.chunkScheduledTicks = [] { return 0; };
    sources.chunkGeneratedEntities = [] { return 0; };
    sources.worldgenCaches = [] { return std::vector<std::int64_t>{}; };
    sources.worldCount = [] { return 0; };
    sources.cpuPercent = [] { return std::optional<double>{}; };
    sources.liveHeapBytes = [] { return std::optional<std::int64_t>{}; };
    sources.residentBytes = [] { return std::optional<std::int64_t>{}; };
    return sources;
}

struct Fixture {
    PlayerRegistry registry;
    std::vector<std::unique_ptr<PlayerSession>> players;
    std::vector<std::unique_ptr<Client>> clients;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<basalt::server::storage::PlayerStore> store;
    basalt::wire::common::Timings timings;
    basalt::wire::common::TickDurations durations;
    std::int64_t clockNanos = 0;
    /// Only what the report actually reads; the rest stays unset and is reported as absent.
    basalt::server::runtime::timings::TimingsReport report;
    CommandEnvironment environment;
    CommandDispatcher dispatcher;
    RecordingSource source;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})),
          store(openStore(root)), report(timings, durations, reportSources(), root + "/timings"),
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
        clients.back()->session = players.back().get();
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

namespace mobEffects = basalt::server::runtime::world::mobEffects;

namespace play = basalt::wire::protocol::play;

using basalt::server::storage::StoredItemStack;

TEST_CASE("timings reports the tick distribution without measuring sections") {
    Fixture fixture{scratch("timings-summary")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimingsCommand>(
        fixture.timings, fixture.durations, fixture.report, [&fixture] { return fixture.clockNanos; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "timings") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Ticks per second:") != std::string::npos);
    CHECK(all.find("Sections are not being measured") != std::string::npos);
    // Three decimals, because a real sub-millisecond tick read as "0.00" and looked like no data.
    CHECK(all.find("0.000") != std::string::npos);
}

TEST_CASE("timings start and stop move the flag the sections are gated on") {
    Fixture fixture{scratch("timings-start")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TimingsCommand>(
        fixture.timings, fixture.durations, fixture.report, [&fixture] { return fixture.clockNanos; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "timings start") == 1);
    CHECK(fixture.timings.enabled());
    CHECK(fixture.dispatcher.execute(fixture.source, "timings stop") == 1);
    CHECK_FALSE(fixture.timings.enabled());
    CHECK(fixture.dispatcher.execute(fixture.source, "timings reset") == 1);
}

TEST_CASE("debug start refuses a second run and stop reports the window") {
    Fixture fixture{scratch("timings-debug")};
    std::int64_t ticks = 0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::DebugCommand>(
        fixture.timings, fixture.report, [&fixture] { return fixture.clockNanos; }, [&ticks] { return ticks; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "debug start") == 1);
    CHECK(fixture.source.lastAsText().find("commands.debug.started") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "debug start") == 0);
    CHECK(fixture.source.lastAsText().find("commands.debug.alreadyRunning") != std::string::npos);

    fixture.clockNanos = 2'000'000'000;
    ticks = 40;
    CHECK(fixture.dispatcher.execute(fixture.source, "debug stop") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("commands.debug.stopped") != std::string::npos);
    // Forty ticks in two seconds is twenty a second, printed with two decimals.
    CHECK(all.find("20.00") != std::string::npos);
}

TEST_CASE("debug stop without a run in flight is refused") {
    Fixture fixture{scratch("timings-debug-stop")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DebugCommand>(
        fixture.timings, fixture.report, [&fixture] { return fixture.clockNanos; }, [] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "debug stop") == 0);
    CHECK(fixture.source.lastAsText().find("commands.debug.notRunning") != std::string::npos);
}

TEST_CASE("debug function needs a runner to hand the function to") {
    Fixture fixture{scratch("timings-debug-function")};
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::DebugCommand>(fixture.timings, fixture.report, [] { return 0; }, [] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "debug function test:greet") == 0);
    CHECK(fixture.source.lastAsText().find("No functions are loaded") != std::string::npos);
}

TEST_CASE("perf starts and stops the same measurements timings uses") {
    Fixture fixture{scratch("timings-perf")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::PerfCommand>(
        fixture.timings, fixture.report, [&fixture] { return fixture.clockNanos; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "perf stop") == 0);
    CHECK(fixture.source.lastAsText().find("commands.perf.notRunning") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "perf start") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "perf start") == 0);
    CHECK(fixture.source.lastAsText().find("commands.perf.alreadyRunning") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "perf stop") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("commands.perf.reportSaved") != std::string::npos);
}

TEST_CASE("jfr answers rather than failing, and points at what does measure") {
    Fixture fixture{scratch("timings-jfr")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::JfrCommand>(fixture.timings));

    // Zero rather than one: nothing was recorded, and `/execute store` should see that.
    CHECK(fixture.dispatcher.execute(fixture.source, "jfr start") == 0);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("does not run on a JVM") != std::string::npos);
    CHECK(all.find("/timings") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "jfr stop") == 0);
}

TEST_CASE("publish answers with the port this dedicated server already listens on") {
    Fixture fixture{scratch("timings-publish")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::PublishCommand>([] { return 25565; }));
    fixture.dispatcher.registerCommand(std::make_shared<commands::UnpublishCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "publish") == 25565);
    CHECK(fixture.source.lastAsText().find("25565") != std::string::npos);
    // Vanilla's own arguments are accepted so the tree is the shape a client expects.
    CHECK(fixture.dispatcher.execute(fixture.source, "publish true survival 25566") == 25565);
    CHECK(fixture.dispatcher.execute(fixture.source, "unpublish") == 1);
}

TEST_CASE("fetchprofile answers from who is connected, and from Mojang for a name") {
    Fixture fixture{scratch("timings-fetchprofile")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::FetchProfileCommand>([](const std::string& name) -> std::optional<std::string> {
            return name == "Notch" ? std::optional<std::string>{"069a79f4-44e9-4726-a5be-fca90e38aaf5"} : std::nullopt;
        }));

    CHECK(fixture.dispatcher.execute(fixture.source, "fetchprofile name Notch") == 1);
    CHECK(fixture.source.lastAsText().find("069a79f4-44e9-4726-a5be-fca90e38aaf5") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "fetchprofile name Nobody") == 0);

    CHECK(fixture.dispatcher.execute(fixture.source, "fetchprofile entity Steve") == 1);
    CHECK(fixture.source.lastAsText().find("Steve is") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "fetchprofile id " + steve.uuid().toString()) == 1);
    CHECK(fixture.source.lastAsText().find("is Steve") != std::string::npos);
}
