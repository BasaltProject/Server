#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/Favicon.hpp>
#include <basalt/server/runtime/PersistenceCoordinator.hpp>
#include <basalt/server/runtime/ServerMetrics.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/PlayerStore.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::PersistenceCoordinator;
using basalt::server::runtime::ServerMetrics;
using basalt::server::runtime::command::CommandStorage;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Uuid;
namespace config = basalt::server::runtime::config;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    return directory.string();
}

struct Fixture {
    std::string root;
    PlayerRegistry registry;
    std::vector<std::unique_ptr<PlayerSession>> players;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<basalt::server::storage::PlayerStore> store;
    CommandStorage commandStorage;
    ServerMetrics metrics;

    explicit Fixture(std::string directory)
        : root(std::move(directory)),
          world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              // The root is where worlds live, not one world: `LoadedWorld` appends its own name.
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})),
          commandStorage(root + "/storage.nbt") {
        auto opened = basalt::server::storage::PlayerStore::open(root + "/players.db");
        REQUIRE(opened.has_value());
        store = std::move(*opened);
    }

    [[nodiscard]] PersistenceCoordinator::Hooks hooks() {
        return {.worlds = [this] { return std::map<std::string, LoadedWorld*>{{"world", world.get()}}; }};
    }

    PlayerSession& add(const std::string& name, bool savable) {
        PlayerConnection connection;
        const auto uuid = Uuid{static_cast<std::uint64_t>(players.size() + 1), 1};
        if (savable) {
            connection.storedSnapshot = [uuid, name] {
                return basalt::server::storage::StoredPlayer{.uuid = uuid.toString(), .name = name, .world = "world"};
            };
        }
        players.push_back(std::make_unique<PlayerSession>(uuid, name, static_cast<std::int32_t>(players.size() + 1),
                                                          std::move(connection)));
        players.back()->worldName = "world";
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

TEST_CASE("saving the players writes everyone who has a snapshot and skips whoever does not") {
    Fixture fixture{scratch("persistence-players")};
    fixture.add("Steve", true);
    // A player still logging in has nothing worth writing, and must not be written as an empty row.
    fixture.add("Alex", false);
    const PersistenceCoordinator coordinator{fixture.hooks(),        fixture.registry, *fixture.store,
                                             fixture.commandStorage, fixture.metrics,  {}};

    coordinator.persistPlayers();
    CHECK(fixture.store->players().loadByName("Steve").has_value());
    CHECK_FALSE(fixture.store->players().loadByName("Alex").has_value());
}

TEST_CASE("a full save writes the worlds and the players in one pass") {
    Fixture fixture{scratch("persistence-everything")};
    fixture.add("Steve", true);
    fixture.world->state().getOrGenerateChunk(0, 0);
    const PersistenceCoordinator coordinator{fixture.hooks(),        fixture.registry, *fixture.store,
                                             fixture.commandStorage, fixture.metrics,  {}};

    coordinator.persistEverything();
    CHECK(fixture.store->players().loadByName("Steve").has_value());
    CHECK(std::filesystem::exists(fixture.root + "/world/level.nbt"));
}

TEST_CASE("the unload sweep only reports chunks that really went, and only when somebody listens") {
    Fixture fixture{scratch("persistence-unload")};
    fixture.world->state().getOrGenerateChunk(40, 40);
    std::vector<std::pair<std::int32_t, std::int32_t>> unloaded;
    PersistenceCoordinator::Hooks hooks = fixture.hooks();
    hooks.wantsUnloadEvents = [] { return true; };
    hooks.chunkUnloaded = [&unloaded](const std::string&, std::int32_t x, std::int32_t z) {
        unloaded.emplace_back(x, z);
    };
    const PersistenceCoordinator coordinator{
        hooks, fixture.registry, *fixture.store, fixture.commandStorage, fixture.metrics, {}};

    // Nobody is in the world, so a chunk far from spawn is unreachable and goes.
    coordinator.unloadUnreachableChunks();
    CHECK(unloaded == std::vector<std::pair<std::int32_t, std::int32_t>>{{40, 40}});

    // A second sweep has nothing left to unload and reports nothing.
    unloaded.clear();
    coordinator.unloadUnreachableChunks();
    CHECK(unloaded.empty());
}

TEST_CASE("the built-in favicon decodes to a real PNG") {
    const std::span<const std::uint8_t> bytes = basalt::server::runtime::favicon::bytes();
    REQUIRE(bytes.size() > 8);
    // The PNG signature, so a truncated or misquoted blob fails here rather than at a client that
    // silently draws nothing.
    const std::vector<std::uint8_t> signature{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    CHECK(std::vector<std::uint8_t>{bytes.begin(), bytes.begin() + 8} == signature);
}
