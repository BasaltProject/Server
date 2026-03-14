#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/TeleportCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
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
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
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
        // The command asks the connection to silence a sound rather than writing the packet itself,
        // because the two doors write different ones.
        connection.stopSound = [this](const std::optional<std::string>&,
                                      const std::optional<basalt::wire::protocol::play::SoundSource>&) {
            packets.push_back(basalt::wire::protocol::play::StopSoundPacket::kId);
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

TEST_CASE("teleport moves whoever it names to the position it was given") {
    Fixture fixture{scratch("teleport-command-position")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve 10 64 -3") == 1);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    // parity: a bare integer in a vec3 is centre corrected, so it lands in the middle of the block.
    CHECK(fixture.clients[0]->teleports[0][0] == doctest::Approx(10.5));
    CHECK(fixture.clients[0]->teleports[0][2] == doctest::Approx(-2.5));
    CHECK(fixture.source.lastAsText().find("commands.teleport.success.location.single") != std::string::npos);
    // parity: `String.format("%f")`, always six decimals.
    CHECK(fixture.source.lastAsText().find("10.500000") != std::string::npos);
}

TEST_CASE("a relative coordinate is relative to whoever runs it, not to the entity being moved") {
    Fixture fixture{scratch("teleport-command-relative")};
    PlayerSession& steve = fixture.add("Steve");
    steve.x = 100.0;
    steve.y = 70.0;
    steve.z = 100.0;
    fixture.source.at = {4.0, 60.0, 4.0};
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    // parity: `TeleportCommand.teleportToPos` resolves against the source, once.
    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve ~ ~5 ~") == 1);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    CHECK(fixture.clients[0]->teleports[0][0] == doctest::Approx(4.0));
    CHECK(fixture.clients[0]->teleports[0][1] == doctest::Approx(65.0));
}

TEST_CASE("teleport to a player lands on them, not on their numbers") {
    Fixture fixture{scratch("teleport-command-player")};
    fixture.add("Steve");
    PlayerSession& alex = fixture.add("Alex");
    alex.x = 8.5;
    alex.y = 70.0;
    alex.z = -4.5;
    alex.yaw = 90.0F;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve Alex") == 1);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    CHECK(fixture.clients[0]->teleports[0][0] == doctest::Approx(8.5));
    CHECK(fixture.clients[0]->teleports[0][3] == doctest::Approx(90.0F));
    CHECK(fixture.source.lastAsText().find("commands.teleport.success.entity.single") != std::string::npos);
}

TEST_CASE("teleport with a rotation keeps it rather than the target's own") {
    Fixture fixture{scratch("teleport-command-rotation")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve 0 64 0 90 45") == 1);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    CHECK(fixture.clients[0]->teleports[0][3] == doctest::Approx(90.0F));
    CHECK(fixture.clients[0]->teleports[0][4] == doctest::Approx(45.0F));
}

TEST_CASE("teleport facing turns the target towards the point it names") {
    Fixture fixture{scratch("teleport-command-facing")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    // Standing at the origin and looking due south, which is +Z, so the yaw is zero.
    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve 0.0 64.0 0.0 facing 0.0 65.62 10.0") == 1);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    CHECK(fixture.clients[0]->teleports[0][3] == doctest::Approx(0.0F).epsilon(0.01));
    CHECK(fixture.clients[0]->teleports[0][4] == doctest::Approx(0.0F).epsilon(0.01));

    // Due west is -X, which vanilla writes as yaw 90.
    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve 0.0 64.0 0.0 facing -10.0 65.62 0.0") == 1);
    CHECK(fixture.clients[0]->teleports[1][3] == doctest::Approx(90.0F).epsilon(0.01));
}

TEST_CASE("teleport facing an entity aims at its feet or its eyes") {
    Fixture fixture{scratch("teleport-command-facing-entity")};
    fixture.add("Steve");
    PlayerSession& alex = fixture.add("Alex");
    // Off to one side, so the two anchors give different pitches: straight overhead is -90 either
    // way, since the horizontal distance is what the pitch is measured against.
    alex.x = 10.0;
    alex.y = 74.0;
    alex.z = 0.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve 0.0 64.0 0.0 facing entity Alex feet") == 1);
    const float atFeet = static_cast<float>(fixture.clients[0]->teleports[0][4]);
    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve 0.0 64.0 0.0 facing entity Alex eyes") == 1);
    const float atEyes = static_cast<float>(fixture.clients[0]->teleports[1][4]);
    // Looking up, so the pitch is negative, and the eyes are higher still.
    CHECK(atFeet < 0.0F);
    CHECK(atEyes < atFeet);
}

TEST_CASE("teleport moves any entity, and takes a player to one") {
    Fixture fixture{scratch("teleport-command-entity")};
    fixture.add("Steve");
    const auto* cowType = basalt::server::registry::entityTypes::of(Identifier::vanilla("cow"));
    REQUIRE(cowType != nullptr);
    ServerEntity& cow = fixture.world->state().spawnEntity(std::make_unique<ServerEntity>(
        fixture.registry.allocateEntityId(), Uuid{9, 9}, cowType->id, cowType->protocolId, 5.0, 64.0, 5.0));
    cow.velocityY = -1.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tp @e[type=cow] 20.0 70.0 -4.0 90 0") == 1);
    CHECK(cow.x == doctest::Approx(20.0));
    CHECK(cow.y == doctest::Approx(70.0));
    CHECK(cow.z == doctest::Approx(-4.0));
    CHECK(cow.yaw == doctest::Approx(90.0F));
    // parity: `performTeleport` leaves it standing, with no fall speed.
    CHECK(cow.velocityY == doctest::Approx(0.0));

    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve @e[type=cow]") == 1);
    REQUIRE(fixture.clients[0]->teleports.size() == 1);
    CHECK(fixture.clients[0]->teleports[0][0] == doctest::Approx(20.0));
    CHECK(fixture.clients[0]->teleports[0][3] == doctest::Approx(90.0F));
}

TEST_CASE("teleport into another world switches rather than moving inside this one") {
    Fixture fixture{scratch("teleport-command-world")};
    PlayerSession& steve = fixture.add("Steve");
    steve.worldName = "elsewhere";
    fixture.dispatcher.registerCommand(std::make_shared<commands::TeleportCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "tp Steve 0 64 0 world") == 1);
    REQUIRE(fixture.clients[0]->switched.size() == 1);
    // The world change re-sends the position itself, so nothing teleports afterwards.
    CHECK(fixture.clients[0]->teleports.empty());
    CHECK(fixture.source.lastAsText().find("in world") != std::string::npos);
}

TEST_CASE("playsound reaches whoever is in earshot and nobody else") {
    Fixture fixture{scratch("playsound-range")};
    PlayerSession& steve = fixture.add("Steve");
    steve.x = 0.0;
    steve.y = 64.0;
    steve.z = 0.0;
    PlayerSession& alex = fixture.add("Alex");
    alex.x = 1000.0;
    alex.y = 64.0;
    alex.z = 0.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaySoundCommand>([] { return 7; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "playsound minecraft:entity.pig.ambient player @a 0 64 0") == 1);
    CHECK(fixture.clients[0]->sent(play::SoundPacket::kId));
    // Out of earshot with no minimum volume, so the far player hears nothing at all.
    CHECK(fixture.clients[1]->packets.empty());
    CHECK(fixture.source.lastAsText().find("commands.playsound.success.single") != std::string::npos);
}

TEST_CASE("a minimum volume moves a distant sound into earshot rather than dropping it") {
    Fixture fixture{scratch("playsound-minvolume")};
    PlayerSession& alex = fixture.add("Alex");
    alex.x = 1000.0;
    alex.y = 64.0;
    alex.z = 0.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaySoundCommand>([] { return 7; }));

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "playsound minecraft:entity.pig.ambient player @a 0 64 0 1 1 0.5") == 1);
    CHECK(fixture.clients[0]->sent(play::SoundPacket::kId));
}

TEST_CASE("playsound that reaches nobody is refused rather than reporting a play of none") {
    Fixture fixture{scratch("playsound-nobody")};
    PlayerSession& alex = fixture.add("Alex");
    alex.x = 1000.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaySoundCommand>([] { return 7; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "playsound minecraft:entity.pig.ambient player @a 0 64 0") == 0);
    CHECK(fixture.source.lastAsText().find("commands.playsound.failed") != std::string::npos);
}

TEST_CASE("stopsound says which half it was given") {
    Fixture fixture{scratch("stopsound")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::StopSoundCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "stopsound Steve") == 1);
    CHECK(fixture.source.lastAsText().find("commands.stopsound.success.sourceless.any") != std::string::npos);
    CHECK(fixture.clients[0]->sent(play::StopSoundPacket::kId));

    CHECK(fixture.dispatcher.execute(fixture.source, "stopsound Steve music") == 1);
    CHECK(fixture.source.lastAsText().find("commands.stopsound.success.source.any") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "stopsound Steve * minecraft:entity.pig.ambient") == 1);
    CHECK(fixture.source.lastAsText().find("commands.stopsound.success.sourceless.sound") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "stopsound Steve music minecraft:music.game") == 1);
    CHECK(fixture.source.lastAsText().find("commands.stopsound.success.source.sound") != std::string::npos);
}
