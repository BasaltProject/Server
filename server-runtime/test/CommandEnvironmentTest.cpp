#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/CommandEnvironment.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::config::WorldDeclaration;
using basalt::server::runtime::config::WorldDimension;
using basalt::server::runtime::config::WorldSettings;
using basalt::server::runtime::config::WorldType;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::SetTimePacket;
using basalt::wire::text::Component;
namespace blockEffect = basalt::server::runtime::world::blockEffect;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::unique_ptr<LoadedWorld> worldNamed(const std::string& name, const std::string& root,
                                                      WorldDimension dimension = WorldDimension::Overworld) {
    WorldMetadata metadata;
    metadata.seed = 7;
    return std::make_unique<LoadedWorld>(
        WorldDeclaration{
            .name = name, .dimension = dimension, .type = WorldType::Flat, .seed = 7, .isDefault = name == "world"},
        WorldSettings{}, metadata, root, basalt::wire::common::Logger{"Test"});
}

/// Everything one player's connection was asked to send.
struct Recorded {
    std::vector<Component> messages;
    std::vector<std::int32_t> packets;
    std::vector<std::pair<std::int32_t, std::int32_t>> resent;
    std::vector<std::pair<std::int32_t, std::int32_t>> biomes;
    std::int32_t weathers = 0;
    std::vector<std::int32_t> effectStates;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.resendChunk = [this](std::int32_t chunkX, std::int32_t chunkZ) {
            resent.emplace_back(chunkX, chunkZ);
        };
        connection.sendBiomes = [this](const std::vector<std::pair<std::int32_t, std::int32_t>>& columns) {
            biomes.insert(biomes.end(), columns.begin(), columns.end());
        };
        connection.sendWeather = [this](const basalt::server::runtime::world::WorldWeather&) { weathers++; };
        connection.broadcastEffects = [this](std::int32_t, std::int32_t, std::int32_t, std::int32_t stateId,
                                             const std::vector<basalt::server::runtime::world::BlockEffect>&) {
            effectStates.push_back(stateId);
        };
        return connection;
    }
};

} // namespace

TEST_CASE("a broadcast reaches every player that is logged in") {
    PlayerRegistry registry;
    Recorded first;
    Recorded second;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, first.connection()};
    PlayerSession alex{Uuid{2, 2}, "Alex", 2, second.connection()};
    registry.add(&steve);
    registry.add(&alex);
    const CommandEnvironment environment{registry, {}};

    environment.broadcast(Component::text("hello"));

    CHECK(first.messages.size() == 1);
    CHECK(second.messages.size() == 1);
}

TEST_CASE("only the players standing in a world are the world's") {
    const std::string root = scratch("command-environment-players");
    const std::unique_ptr<LoadedWorld> world = worldNamed("world", root);
    const std::unique_ptr<LoadedWorld> nether = worldNamed("nether", root, WorldDimension::Nether);
    PlayerRegistry registry;
    Recorded here;
    Recorded there;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, here.connection()};
    PlayerSession alex{Uuid{2, 2}, "Alex", 2, there.connection()};
    steve.worldName = "world";
    alex.worldName = "nether";
    registry.add(&steve);
    registry.add(&alex);
    const CommandEnvironment environment{registry, {}};

    REQUIRE(environment.playersIn(*world).size() == 1);
    CHECK(environment.playersIn(*world)[0] == &steve);
    CHECK(environment.playersIn(*nether)[0] == &alex);
}

TEST_CASE("the clock a world sends is its own, and the End has a clock of its own too") {
    const std::string root = scratch("command-environment-time");
    const std::unique_ptr<LoadedWorld> overworld = worldNamed("world", root);
    const std::unique_ptr<LoadedWorld> end = worldNamed("end", root, WorldDimension::End);

    const SetTimePacket fromOverworld = basalt::server::runtime::command::timePacketOf(*overworld);
    REQUIRE(fromOverworld.clocks.size() == 1);
    CHECK(fromOverworld.clocks[0].clockId == SetTimePacket::kOverworldClock);

    const SetTimePacket fromEnd = basalt::server::runtime::command::timePacketOf(*end);
    CHECK(fromEnd.clocks[0].clockId == SetTimePacket::kEndClock);
}

TEST_CASE("a time sync reaches the world's own players and nobody else") {
    const std::string root = scratch("command-environment-sync-time");
    const std::unique_ptr<LoadedWorld> world = worldNamed("world", root);
    PlayerRegistry registry;
    Recorded here;
    Recorded elsewhere;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, here.connection()};
    PlayerSession alex{Uuid{2, 2}, "Alex", 2, elsewhere.connection()};
    steve.worldName = "world";
    alex.worldName = "somewhere_else";
    registry.add(&steve);
    registry.add(&alex);
    const CommandEnvironment environment{registry, {}};

    environment.syncTime(*world);

    REQUIRE(here.packets.size() == 1);
    CHECK(here.packets[0] == SetTimePacket::kId);
    CHECK(elsewhere.packets.empty());
}

TEST_CASE("a chunk resync covers every column the block range touches") {
    const std::string root = scratch("command-environment-chunks");
    const std::unique_ptr<LoadedWorld> world = worldNamed("world", root);
    PlayerRegistry registry;
    Recorded recorded;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, recorded.connection()};
    steve.worldName = "world";
    registry.add(&steve);
    const CommandEnvironment environment{registry, {}};

    environment.syncChunks(*world, 0, 0, 31, 15);

    // Two columns across and one down: (0,0), (0,1) is out of range, so (0,0) and (1,0).
    REQUIRE(recorded.resent.size() == 2);
    CHECK(recorded.resent[0] == std::pair{0, 0});
    CHECK(recorded.resent[1] == std::pair{1, 0});
}

TEST_CASE("a biome resync sends every column at once rather than one at a time") {
    const std::string root = scratch("command-environment-biomes");
    const std::unique_ptr<LoadedWorld> world = worldNamed("world", root);
    PlayerRegistry registry;
    Recorded recorded;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, recorded.connection()};
    steve.worldName = "world";
    registry.add(&steve);
    const CommandEnvironment environment{registry, {}};

    environment.syncBiomes(*world, -16, -16, 15, 15);

    CHECK(recorded.biomes.size() == 4);
}

TEST_CASE("the weather goes out once per player in the world") {
    const std::string root = scratch("command-environment-weather");
    const std::unique_ptr<LoadedWorld> world = worldNamed("world", root);
    PlayerRegistry registry;
    Recorded recorded;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, recorded.connection()};
    steve.worldName = "world";
    registry.add(&steve);
    const CommandEnvironment environment{registry, {}};

    environment.syncWeather(*world);

    CHECK(recorded.weathers == 1);
}

TEST_CASE("a block effect goes out through one connection, which tells the whole world") {
    const std::string root = scratch("command-environment-effects");
    const std::unique_ptr<LoadedWorld> world = worldNamed("world", root);
    PlayerRegistry registry;
    Recorded first;
    Recorded second;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, first.connection()};
    PlayerSession alex{Uuid{2, 2}, "Alex", 2, second.connection()};
    steve.worldName = "world";
    alex.worldName = "world";
    registry.add(&steve);
    registry.add(&alex);
    const CommandEnvironment environment{registry, {}};

    environment.blockEffects(*world, 1, 2, 3, 42, {blockEffect::Event{.b0 = 1, .b1 = 0}});

    // One is enough: the connection sends it to everyone in its own world.
    CHECK(first.effectStates.size() + second.effectStates.size() == 1);

    // Nothing to say is nothing sent.
    environment.blockEffects(*world, 1, 2, 3, 42, {});
    CHECK(first.effectStates.size() + second.effectStates.size() == 1);
}

TEST_CASE("the groups and the commands are read live, so a reload is seen") {
    PlayerRegistry registry;
    basalt::server::runtime::config::GroupsConfig first{{}, 0, 0};
    basalt::server::runtime::config::GroupsConfig second{
        {basalt::server::runtime::config::PermissionGroup{.name = "default", .isDefault = true}}, 0, 0};
    const basalt::server::runtime::config::GroupsConfig* current = &first;
    CommandEnvironment::Options options;
    options.groups = [&current]() -> const basalt::server::runtime::config::GroupsConfig& { return *current; };
    const CommandEnvironment environment{registry, std::move(options)};

    CHECK(&environment.groups() == &first);
    current = &second;
    CHECK(&environment.groups() == &second);
}

TEST_CASE("an environment with nothing handed to it still answers") {
    PlayerRegistry registry;
    const CommandEnvironment environment{registry, {}};

    CHECK(environment.pluginEvents() == nullptr);
    CHECK(environment.commands().messages().empty());
    CHECK(environment.placeholders().identifiers().empty());
    CHECK(environment.options().maxPlayers == 20);
    CHECK(environment.options().defaultWorldName == "world");
    // A broadcast with nobody logged in still reaches the log rather than failing.
    environment.broadcast(Component::text("nobody hears this"));
}
