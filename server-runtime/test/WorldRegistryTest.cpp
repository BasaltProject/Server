#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/WorldRegistry.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using basalt::server::runtime::WorldRegistry;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::world::LoadedWorld;
namespace config = basalt::server::runtime::config;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

/// One overworld and its Nether, the way `worlds.toml` declares a pair.
[[nodiscard]] config::WorldsConfig pairedWorlds() {
    return config::WorldsConfig{{config::WorldDeclaration{.name = "world",
                                                          .dimension = config::WorldDimension::Overworld,
                                                          .type = config::WorldType::Flat,
                                                          .seed = 4242,
                                                          .isDefault = true},
                                 config::WorldDeclaration{.name = "world_nether",
                                                          .dimension = config::WorldDimension::Nether,
                                                          .type = config::WorldType::Flat,
                                                          .isDefault = false}},
                                config::WorldSettings{},
                                "worlds"};
}

} // namespace

TEST_CASE("a dimension with no seed of its own takes its overworld's") {
    const config::BasaltConfig config;
    const config::WorldsConfig worlds = pairedWorlds();
    PlayerRegistry players;
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot = scratch("registry-seed")}};

    const std::map<std::string, LoadedWorld*> all = registry.worlds();
    REQUIRE(all.size() == 2);
    // parity: a vanilla save has one seed for its three dimensions, which is what makes them one
    // world rather than three unrelated ones.
    CHECK(all.at("world")->metadata().seed == 4242);
    CHECK(all.at("world_nether")->metadata().seed == 4242);
    CHECK(registry.worldSeed() == 4242);
    CHECK(&registry.defaultWorld() == all.at("world"));
    // parity: the default save's dimensions answer to vanilla's names, which datapacks write.
    CHECK(all.at("world")->levelKey() == "minecraft:overworld");
    CHECK(all.at("world_nether")->levelKey() == "minecraft:the_nether");
    CHECK(registry.createWorld("arena", config::WorldDimension::Nether, config::WorldType::Flat, 7)->levelKey() ==
          "basalt:arena");
}

TEST_CASE("a world made at runtime is a world like any other, and its name has to be usable") {
    const config::BasaltConfig config;
    const config::WorldsConfig worlds = pairedWorlds();
    PlayerRegistry players;
    std::vector<std::string> loaded;
    WorldRegistry registry{config,
                           worlds,
                           nullptr,
                           players,
                           {.worldLoaded = [&loaded](const std::string& name) { loaded.push_back(name); }},
                           {.worldsRoot = scratch("registry-create")}};

    LoadedWorld* made = registry.createWorld("arena", config::WorldDimension::Overworld, config::WorldType::Flat, 7);
    REQUIRE(made != nullptr);
    CHECK(made->metadata().seed == 7);
    CHECK(registry.worlds().size() == 3);

    // A name already taken, and a name that is not a folder name, are both refused outright: a
    // world that half exists is worse than none.
    CHECK(registry.createWorld("arena", config::WorldDimension::Overworld, config::WorldType::Flat, 7) == nullptr);
    CHECK(registry.createWorld("../escape", config::WorldDimension::Overworld, config::WorldType::Flat, 7) == nullptr);
    CHECK(registry.createWorld("", config::WorldDimension::Overworld, config::WorldType::Flat, 7) == nullptr);
    CHECK(registry.worlds().size() == 3);

    // The event is scheduled rather than fired, so a plugin's own handler is not re-entered.
    CHECK(loaded.empty());
}

TEST_CASE("reloading the configuration rebuilds the command tree") {
    const config::BasaltConfig config;
    const config::WorldsConfig worlds = pairedWorlds();
    PlayerRegistry players;
    std::int32_t rebuilds = 0;
    WorldRegistry registry{config,
                           worlds,
                           nullptr,
                           players,
                           {.rebuildCommands = [&rebuilds] { rebuilds++; }},
                           {.worldsRoot = scratch("registry-reload")}};

    registry.onConfigsReloaded();
    CHECK(rebuilds == 1);
}
