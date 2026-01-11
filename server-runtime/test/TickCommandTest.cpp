#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/StopwatchCommand.hpp>
#include <basalt/server/runtime/command/commands/TickCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
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
    std::int64_t clockMillis = 0;
    basalt::server::runtime::TickLoop tickLoop;
    basalt::wire::common::TickDurations durations;
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

namespace play = basalt::wire::protocol::play;

TEST_CASE("a stopwatch counts wall clock seconds from when it was made") {
    Fixture fixture{scratch("tick-stopwatch")};
    commands::Stopwatches stopwatches{[&fixture] { return fixture.clockMillis; }};
    fixture.dispatcher.registerCommand(std::make_shared<commands::StopwatchCommand>(stopwatches));

    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch create test:run") == 1);
    fixture.clockMillis = 4500;
    // The scale is what `/execute store` reads, so a query truncates rather than rounds.
    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch query test:run") == 4);
    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch query test:run 1000") == 4500);
    CHECK(fixture.source.lastAsText().find("commands.stopwatch.query") != std::string::npos);
}

TEST_CASE("a stopwatch cannot be made twice and cannot be read before it exists") {
    Fixture fixture{scratch("tick-stopwatch-twice")};
    commands::Stopwatches stopwatches{[&fixture] { return fixture.clockMillis; }};
    fixture.dispatcher.registerCommand(std::make_shared<commands::StopwatchCommand>(stopwatches));

    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch query test:run") == 0);
    CHECK(fixture.source.lastAsText().find("commands.stopwatch.does_not_exist") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch create test:run") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch create test:run") == 0);
    CHECK(fixture.source.lastAsText().find("commands.stopwatch.already_exists") != std::string::npos);
}

TEST_CASE("restart puts a stopwatch back to zero and remove forgets it") {
    Fixture fixture{scratch("tick-stopwatch-restart")};
    commands::Stopwatches stopwatches{[&fixture] { return fixture.clockMillis; }};
    fixture.dispatcher.registerCommand(std::make_shared<commands::StopwatchCommand>(stopwatches));

    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch create test:run") == 1);
    fixture.clockMillis = 9000;
    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch restart test:run") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch query test:run") == 0);

    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch remove test:run") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "stopwatch remove test:run") == 0);
    CHECK(fixture.source.lastAsText().find("commands.stopwatch.does_not_exist") != std::string::npos);
}

TEST_CASE("tick rate is clamped to the bounds the argument declares") {
    Fixture fixture{scratch("tick-rate")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::TickCommand>(fixture.tickLoop, fixture.durations, [] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "tick rate 40") == 40);
    CHECK(fixture.tickLoop.tickRate() == doctest::Approx(40.0F));
    // Out of range never reaches the loop: the client validates with the same bounds.
    CHECK(fixture.dispatcher.execute(fixture.source, "tick rate 0.5") == 0);
    CHECK(fixture.tickLoop.tickRate() == doctest::Approx(40.0F));
    // The client draws its own frozen overlay, so a rate change has to reach it too.
    CHECK(fixture.clients[0]->sent(play::TickingStatePacket::kId));
}

TEST_CASE("tick freeze and unfreeze move the flag the client is told about") {
    Fixture fixture{scratch("tick-freeze")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::TickCommand>(fixture.tickLoop, fixture.durations, [] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "tick freeze") == 1);
    CHECK(fixture.tickLoop.isFrozen());
    CHECK(fixture.source.lastAsText().find("commands.tick.status.frozen") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "tick unfreeze") == 1);
    CHECK_FALSE(fixture.tickLoop.isFrozen());
    CHECK(fixture.source.lastAsText().find("commands.tick.status.running") != std::string::npos);
}

TEST_CASE("tick step needs a frozen loop, as vanilla does") {
    Fixture fixture{scratch("tick-step")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::TickCommand>(fixture.tickLoop, fixture.durations, [] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "tick step 5") == 0);
    CHECK(fixture.source.lastAsText().find("commands.tick.step.fail") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "tick freeze"));
    CHECK(fixture.dispatcher.execute(fixture.source, "tick step 5") == 5);
    CHECK(fixture.clients[0]->sent(play::TickingStepPacket::kId));
    CHECK(fixture.dispatcher.execute(fixture.source, "tick step") == 1);
}

TEST_CASE("tick sprint stop refuses when nothing is sprinting") {
    Fixture fixture{scratch("tick-sprint")};
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::TickCommand>(fixture.tickLoop, fixture.durations, [] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "tick sprint stop") == 0);
    CHECK(fixture.source.lastAsText().find("commands.tick.sprint.stop.fail") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "tick sprint 100") == 100);
    CHECK(fixture.tickLoop.isSprinting());
    CHECK(fixture.dispatcher.execute(fixture.source, "tick sprint stop") == 1);
    CHECK_FALSE(fixture.tickLoop.isSprinting());
}

TEST_CASE("tick query reports the rate and the state it is in") {
    Fixture fixture{scratch("tick-query")};
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::TickCommand>(fixture.tickLoop, fixture.durations, [] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "tick query") == 20);
    CHECK(fixture.source.said.size() == 2);
    CHECK(flatten(fixture.source.said[0]).find("commands.tick.query.rate.running") != std::string::npos);
    // parity: one decimal place, so twenty a second reads as "20.0" and its budget as "50.0".
    CHECK(flatten(fixture.source.said[0]).find("20.0") != std::string::npos);
    CHECK(flatten(fixture.source.said[0]).find("50.0") != std::string::npos);
    CHECK(flatten(fixture.source.said[1]).find("commands.tick.status.running") != std::string::npos);
}

TEST_CASE("a frozen loop still owes the steps it was given, and unfreezing forgets them") {
    basalt::server::runtime::TickLoop loop;
    CHECK_FALSE(loop.step(3));
    loop.setFrozen(true);
    CHECK(loop.step(3));
    loop.setFrozen(false);
    // Unfreezing drops the debt rather than running it out at speed.
    CHECK_FALSE(loop.step(3));
}

TEST_CASE("the loop actually ticks, and stops when it is told to") {
    std::atomic<std::int32_t> ticks{0};
    basalt::server::runtime::TickLoop loop{{.saveIntervalSeconds = 0, .onTick = [&ticks](bool worldRuns) {
                                                if (worldRuns) {
                                                    ticks.fetch_add(1);
                                                }
                                            }}};
    loop.start();
    while (ticks.load() < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    loop.stop();
    const std::int32_t counted = ticks.load();
    CHECK(counted >= 3);
    CHECK(loop.ticksRun() >= 3);
    std::this_thread::sleep_for(std::chrono::milliseconds{120});
    CHECK(ticks.load() == counted);
}

TEST_CASE("a frozen loop still calls back, so the console keeps working") {
    std::atomic<std::int32_t> turns{0};
    std::atomic<std::int32_t> worked{0};
    basalt::server::runtime::TickLoop loop{{.saveIntervalSeconds = 0, .onTick = [&turns, &worked](bool worldRuns) {
                                                turns.fetch_add(1);
                                                if (worldRuns) {
                                                    worked.fetch_add(1);
                                                }
                                            }}};
    loop.setFrozen(true);
    loop.start();
    while (turns.load() < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    loop.stop();
    // parity: vanilla keeps serving the console while frozen and only stops the world. The callback
    // has to run for a queued command to land; what it must not do is the world's turn.
    CHECK(turns.load() >= 3);
    CHECK(worked.load() == 0);
}

TEST_CASE("a frozen loop keeps its heartbeat but does no work") {
    std::atomic<std::int32_t> ticks{0};
    basalt::server::runtime::TickLoop loop{{.saveIntervalSeconds = 0, .onTick = [&ticks](bool worldRuns) {
                                                if (worldRuns) {
                                                    ticks.fetch_add(1);
                                                }
                                            }}};
    loop.setFrozen(true);
    loop.start();
    while (loop.ticksRun() < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    loop.stop();
    // The loop counted its turns; the work is what was skipped, which is why /tick unfreeze lands.
    CHECK(ticks.load() == 0);
}
