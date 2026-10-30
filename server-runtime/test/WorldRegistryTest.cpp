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

    // A taken name and a name that is not a folder name are refused: a half-existing world is worse than none.
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

TEST_CASE("unloaded worlds fence old storage and reload from disk after deletion") {
    const config::BasaltConfig config;
    const config::WorldsConfig worlds = pairedWorlds();
    PlayerRegistry players;
    const auto root = scratch("registry-retire");
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot=root}};
    auto* old = registry.createWorld("temporary",config::WorldDimension::Overworld,config::WorldType::Flat,7);
    REQUIRE(old != nullptr);
    old->clock().setTotalTicks(1234);
    REQUIRE(registry.unloadWorld("temporary",true));
    CHECK_FALSE(registry.worlds().contains("temporary"));
    auto* reloaded = registry.createWorld("temporary",config::WorldDimension::Overworld,config::WorldType::Flat,99);
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded != old);
    CHECK(reloaded->metadata().seed == 7);
    CHECK(reloaded->clock().totalTicks() == 1234);
    reloaded->clock().setTotalTicks(9999);
    REQUIRE(registry.unloadWorld("temporary",false));
    std::filesystem::remove_all(reloaded->worldRoot());
    reloaded->saveDirty(); reloaded->saveMetadata(); reloaded->flushRegions();
    CHECK_FALSE(std::filesystem::exists(reloaded->worldRoot()));
    auto* fresh = registry.createWorld("temporary",config::WorldDimension::Overworld,config::WorldType::Flat,99);
    REQUIRE(fresh != nullptr);
    CHECK(fresh->metadata().seed == 99);
    CHECK(fresh->clock().totalTicks() != 9999);
    CHECK_FALSE(registry.unloadWorld("world", false));
}

TEST_CASE("dynamic dimensions use modern storage while existing flat saves are retained") {
    const config::BasaltConfig config;
    const config::WorldsConfig worlds=pairedWorlds();
    PlayerRegistry players;
    const auto root=scratch("registry-dimension-layout");
    WorldRegistry registry{config,worlds,nullptr,players,{}, {.worldsRoot=root}};
    auto* made=registry.createWorld("runtime",config::WorldDimension::Overworld,config::WorldType::Flat,5);
    REQUIRE(made!=nullptr);
    CHECK(made->worldRoot()==root+"/world/dimensions/minecraft/runtime");
    const auto legacy=root+"/legacy";
    const auto legacyMetadata = basalt::server::runtime::world::WorldMetadata::loadOrCreate(legacy,77,
        basalt::server::runtime::world::WorldMetadata::kDefaultSpawn,basalt::wire::common::Logger{"test"});
    CHECK(legacyMetadata.seed == 77);
    auto* old=registry.createWorld("legacy",config::WorldDimension::Overworld,config::WorldType::Flat,99);
    REQUIRE(old!=nullptr);CHECK(old->worldRoot()==legacy);CHECK(old->metadata().seed==77);
    const auto imported=root+"/world/dimensions/minecraft/imported";
    const auto importedMetadata = basalt::server::runtime::world::WorldMetadata::loadOrCreate(imported,88,
        basalt::server::runtime::world::WorldMetadata::kDefaultSpawn,basalt::wire::common::Logger{"test"});
    CHECK(importedMetadata.seed == 88);
    auto* restored=registry.createWorld("imported",config::WorldDimension::Overworld,config::WorldType::Flat,99);
    REQUIRE(restored!=nullptr);CHECK(restored->worldRoot()==imported);CHECK(restored->metadata().seed==88);
}
