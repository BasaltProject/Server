#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/VanillaCommands.hpp>
#include <basalt/server/runtime/command/commands/ReloadCommand.hpp>
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
#include <set>
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

using basalt::server::runtime::command::registerVanillaCommands;
using basalt::server::runtime::command::VanillaCommandHooks;
using basalt::server::runtime::command::vanillaCommands;

TEST_CASE("the bare set registers everything that needs nothing from a running server") {
    Fixture fixture{scratch("vanilla-commands-bare")};
    registerVanillaCommands(fixture.dispatcher, VanillaCommandHooks{});

    // A few from either end of the list, so a reordering does not quietly drop the tail.
    CHECK(fixture.dispatcher.execute(fixture.source, "help") > 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "list") >= 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "version") > 0);
    // Registered but with nothing to act on, which is a refusal rather than a missing command.
    CHECK(fixture.dispatcher.execute(fixture.source, "kill Nobody") == 0);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Unknown command") == std::string::npos);
}

TEST_CASE("a command whose dependency is absent is not registered at all") {
    Fixture fixture{scratch("vanilla-commands-gaps")};
    registerVanillaCommands(fixture.dispatcher, VanillaCommandHooks{});

    // Without a scoreboard there is nothing for `/scoreboard` to read, so the tree never grows it.
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives list") == 0);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Unknown command") != std::string::npos);
}

TEST_CASE("handing over the dependencies grows the tree by exactly those commands") {
    Fixture fixture{scratch("vanilla-commands-hooks")};
    basalt::server::runtime::scoreboard::Scoreboard scoreboard;
    basalt::server::runtime::scoreboard::Teams teams;
    VanillaCommandHooks hooks;
    const std::size_t bare = vanillaCommands(hooks).size();
    hooks.players = &fixture.registry;
    hooks.scoreboard = &scoreboard;
    hooks.teams = &teams;
    hooks.worldSeed = [] { return std::int64_t{42}; };
    hooks.nowMillis = [] { return std::int64_t{0}; };
    hooks.nowEpochSeconds = [] { return std::int64_t{0}; };
    hooks.nextDouble = [] { return 0.5; };
    hooks.nextSoundSeed = [] { return std::int64_t{7}; };
    hooks.setIdleTimeout = [](std::int32_t) {};
    hooks.whitelistState = [] { return false; };
    hooks.setWhitelistState = [](bool) {};

    CHECK(vanillaCommands(hooks).size() > bare);
    registerVanillaCommands(fixture.dispatcher, hooks);
    CHECK(fixture.dispatcher.execute(fixture.source, "scoreboard objectives list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.scoreboard.objectives.list.empty") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "team list") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "banlist") == 0);
}

TEST_CASE("no two commands claim the same name") {
    std::set<std::string> seen;
    for (const auto& command : vanillaCommands(VanillaCommandHooks{})) {
        CHECK(seen.insert(command->name()).second);
        for (const std::string& alias : command->aliases()) {
            CHECK(seen.insert(alias).second);
        }
    }
}

TEST_CASE("reload re-reads the files it can and says which it will not") {
    Fixture fixture{scratch("vanilla-commands-reload")};
    const std::string root = scratch("vanilla-commands-reload-config");
    std::error_code ignored;
    std::filesystem::create_directories(root, ignored);
    const config::ConfigDirectory directory{root};
    // Read once from an empty directory, which writes the defaults and gives a snapshot to swap.
    std::expected<config::ConfigSnapshot, std::string> initial = config::LiveConfigs::read(directory);
    REQUIRE(initial.has_value());
    config::LiveConfigs live{std::move(*initial)};
    std::int32_t reloads = 0;
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::ReloadCommand>(live, [&reloads] { reloads++; }, [] { return 3; }, directory));

    CHECK(fixture.dispatcher.execute(fixture.source, "reload") ==
          static_cast<std::int32_t>(config::LiveConfigs::kReloadableFiles.size()));
    CHECK(reloads == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("3 function(s)") != std::string::npos);
    CHECK(all.find("server.toml and worlds.toml are NOT reloaded") != std::string::npos);

    // The `confirm` form is the same thing, since nothing here needs confirming.
    CHECK(fixture.dispatcher.execute(fixture.source, "reload confirm") > 0);
    CHECK(reloads == 2);
}
