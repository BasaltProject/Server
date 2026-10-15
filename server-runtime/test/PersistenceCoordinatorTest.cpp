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
