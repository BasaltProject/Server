#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/WorldBorderCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
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
    std::vector<std::array<double, 5>> teleports;
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
        connection.teleportTo = [this](double x, double y, double z, float yaw, float pitch) {
            teleports.push_back({x, y, z, static_cast<double>(yaw), static_cast<double>(pitch)});
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

using basalt::server::runtime::world::WorldBorder;

TEST_CASE("worldborder set moves the border and tells whoever is in the world") {
    Fixture fixture{scratch("worldborder-set")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldBorderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder set 100") > 0);
    CHECK(fixture.world->border().targetSize() == doctest::Approx(100.0));
    CHECK(fixture.clients[0]->sent(play::SetBorderSizePacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.worldborder.set.immediate") != std::string::npos);
    // parity: two decimals through `String.format`.
    CHECK(fixture.source.lastAsText().find("100.00") != std::string::npos);
}

TEST_CASE("worldborder set with a time interpolates instead of jumping") {
    Fixture fixture{scratch("worldborder-lerp")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldBorderCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "worldborder set 100"));
    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder set 200 10") > 0);
    CHECK(fixture.clients[0]->sent(play::SetBorderLerpSizePacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.worldborder.set.grow") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder set 50 10") > 0);
    CHECK(fixture.source.lastAsText().find("commands.worldborder.set.shrink") != std::string::npos);
}

TEST_CASE("worldborder refuses a size that would not change, or is out of range") {
    Fixture fixture{scratch("worldborder-refuse")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldBorderCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "worldborder set 100"));
    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder set 100") == 0);
    CHECK(fixture.source.lastAsText().find("commands.worldborder.set.failed.nochange") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder set 0.5") == 0);
    CHECK(fixture.source.lastAsText().find("commands.worldborder.set.failed.small") != std::string::npos);
}

TEST_CASE("worldborder add moves relative to the size in force") {
    Fixture fixture{scratch("worldborder-add")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldBorderCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "worldborder set 100"));
    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder add 50") > 0);
    CHECK(fixture.world->border().targetSize() == doctest::Approx(150.0));
    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder get") == 150);
}

TEST_CASE("worldborder center moves it and refuses a repeat") {
    Fixture fixture{scratch("worldborder-center")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldBorderCommand>());

    // parity: a vec2 never centre corrects, so 10 20 is exactly 10, 20.
    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder center 10 20") == 0);
    CHECK(fixture.world->border().centerX() == doctest::Approx(10.0));
    CHECK(fixture.world->border().centerZ() == doctest::Approx(20.0));
    CHECK(fixture.clients[0]->sent(play::SetBorderCenterPacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.worldborder.center.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder center 10 20") == 0);
    CHECK(fixture.source.lastAsText().find("commands.worldborder.center.failed") != std::string::npos);
}

TEST_CASE("worldborder damage stores both numbers and refuses a repeat") {
    Fixture fixture{scratch("worldborder-damage")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldBorderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder damage amount 1.5") == 0);
    CHECK(fixture.world->border().damagePerBlock == doctest::Approx(1.5));
    CHECK(fixture.source.lastAsText().find("commands.worldborder.damage.amount.success") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder damage amount 1.5") == 0);
    CHECK(fixture.source.lastAsText().find("commands.worldborder.damage.amount.failed") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder damage buffer 2") == 0);
    CHECK(fixture.world->border().damageBuffer == doctest::Approx(2.0));
}

TEST_CASE("worldborder warning pushes both numbers to the client") {
    Fixture fixture{scratch("worldborder-warning")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldBorderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder warning distance 12") == 0);
    CHECK(fixture.world->border().warningBlocks == 12);
    CHECK(fixture.clients[0]->sent(play::SetBorderWarningDistancePacket::kId));

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder warning time 30") == 0);
    CHECK(fixture.world->border().warningSeconds == 30);
    CHECK(fixture.clients[0]->sent(play::SetBorderWarningDelayPacket::kId));

    CHECK(fixture.dispatcher.execute(fixture.source, "worldborder warning time 30") == 0);
    CHECK(fixture.source.lastAsText().find("commands.worldborder.warning.time.failed") != std::string::npos);
}
