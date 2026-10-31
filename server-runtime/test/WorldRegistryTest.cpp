#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/WorldRegistry.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
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

    CHECK(registry.createWorld("arena", config::WorldDimension::Overworld, config::WorldType::Flat, 7) == nullptr);
    CHECK(registry.createWorld("../escape", config::WorldDimension::Overworld, config::WorldType::Flat, 7) == nullptr);
    CHECK(registry.createWorld("", config::WorldDimension::Overworld, config::WorldType::Flat, 7) == nullptr);
    CHECK(registry.worlds().size() == 3);

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
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot = root}};
    auto* old = registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 7);
    REQUIRE(old != nullptr);
    auto keepOld = old->keepAlive();
    old->clock().setTotalTicks(1234);
    REQUIRE(registry.unloadWorld("temporary", true));
    CHECK_FALSE(registry.worlds().contains("temporary"));
    auto* reloaded = registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 99);
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded != old);
    CHECK(reloaded->metadata().seed == 7);
    CHECK(reloaded->clock().totalTicks() == 1234);
    auto keepReloaded = reloaded->keepAlive();
    reloaded->clock().setTotalTicks(9999);
    REQUIRE(registry.unloadWorld("temporary", false));
    std::filesystem::remove_all(reloaded->worldRoot());
    reloaded->saveDirty();
    reloaded->saveMetadata();
    reloaded->flushRegions();
    CHECK_FALSE(std::filesystem::exists(reloaded->worldRoot()));
    auto* fresh = registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 99);
    REQUIRE(fresh != nullptr);
    CHECK(fresh->metadata().seed == 99);
    CHECK(fresh->clock().totalTicks() != 9999);
    CHECK_FALSE(registry.unloadWorld("world", false));
}

TEST_CASE("dynamic dimensions use modern storage while existing flat saves are retained") {
    const config::BasaltConfig config;
    const config::WorldsConfig worlds = pairedWorlds();
    PlayerRegistry players;
    const auto root = scratch("registry-dimension-layout");
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot = root}};
    auto* made = registry.createWorld("runtime", config::WorldDimension::Overworld, config::WorldType::Flat, 5);
    REQUIRE(made != nullptr);
    CHECK(std::filesystem::path{made->worldRoot()} ==
          std::filesystem::path{root + "/world/dimensions/minecraft/runtime"});
    const auto legacy = root + "/legacy";
    const auto legacyMetadata = basalt::server::runtime::world::WorldMetadata::loadOrCreate(
        legacy, 77, basalt::server::runtime::world::WorldMetadata::kDefaultSpawn, basalt::wire::common::Logger{"test"});
    CHECK(legacyMetadata.seed == 77);
    auto* old = registry.createWorld("legacy", config::WorldDimension::Overworld, config::WorldType::Flat, 99);
    REQUIRE(old != nullptr);
    CHECK(std::filesystem::path{old->worldRoot()} == std::filesystem::path{legacy});
    CHECK(old->metadata().seed == 77);
    const auto imported = root + "/world/dimensions/minecraft/imported";
    const auto importedMetadata = basalt::server::runtime::world::WorldMetadata::loadOrCreate(
        imported, 88, basalt::server::runtime::world::WorldMetadata::kDefaultSpawn,
        basalt::wire::common::Logger{"test"});
    CHECK(importedMetadata.seed == 88);
    auto* restored = registry.createWorld("imported", config::WorldDimension::Overworld, config::WorldType::Flat, 99);
    REQUIRE(restored != nullptr);
    CHECK(std::filesystem::path{restored->worldRoot()} == std::filesystem::path{imported});
    CHECK(restored->metadata().seed == 88);
}

TEST_CASE("unloaded world memory is released when the last real plugin view is released") {
    const config::BasaltConfig config;
    const auto worlds = pairedWorlds();
    PlayerRegistry players;
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot = scratch("registry-lifetimes")}};
    auto* world = registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 7);
    REQUIRE(world != nullptr);
    std::weak_ptr<LoadedWorld> weak = world->keepAlive();
    namespace model = basalt::server::runtime::plugins;
    model::ModelHooks hooks;
    hooks.worldFor = [&registry](const std::string& name) {
        const auto all = registry.worlds();
        const auto found = all.find(name);
        return found == all.end() ? nullptr : found->second;
    };
    model::LoadedWorlds api{hooks, [] { return std::vector<basalt::server::runtime::player::PlayerSession*>{}; }};
    auto view = api.byName("temporary");
    REQUIRE(view != nullptr);
    world->owner().schedule(1200, [held = world->keepAlive()] {});
    int completions = 0;
    view->requestChunk(8000, 8000, true, 0, [&completions](bool) { ++completions; });
    int lightCompletions = 0;
    const std::array<std::int32_t, 2> lightChunk{8000, 8000};
    view->lightJob(lightChunk, [&lightCompletions](std::int32_t) { ++lightCompletions; });
    REQUIRE(registry.unloadWorld("temporary", false));
    CHECK(completions == 1);
    CHECK(lightCompletions == 1);
    int relit = -1;
    view->lightJob({}, [&relit](std::int32_t count) { relit = count; });
    CHECK(relit == 0);
    CHECK_FALSE(weak.expired());
    CHECK(view->name() == "temporary");
    CHECK(view->env() != nullptr);
    view.reset();
    CHECK(completions == 1);
    CHECK(lightCompletions == 1);
    CHECK(weak.expired());
    for (int i = 0; i < 8; ++i) {
        auto* next = registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 7);
        REQUIRE(next != nullptr);
        weak = next->keepAlive();
        REQUIRE(registry.unloadWorld("temporary", false));
        CHECK(weak.expired());
    }
}

TEST_CASE("managed deletion rejects loaded default unknown and escaped world paths") {
    namespace fs = std::filesystem;
    const config::BasaltConfig config;
    const auto worlds = pairedWorlds();
    PlayerRegistry players;
    const auto root = scratch("registry-delete");
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot = root}};
    for (const std::string& denied : {"world", "world_nether", "missing", "../outside", "/tmp", ""}) {
        CHECK_FALSE(registry.deleteWorld(denied));
    }
    auto* world = registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 7);
    REQUIRE(world != nullptr);
    const fs::path folder = world->worldRoot();
    REQUIRE(fs::exists(folder));
    CHECK_FALSE(registry.deleteWorld("temporary"));
    REQUIRE(registry.unloadWorld("temporary", true));
    const fs::path moved = folder.string() + "-backup";
    fs::rename(folder, moved);
    const fs::path outside = fs::absolute(scratch("registry-delete-outside"));
    fs::create_directories(outside);
    std::ofstream{outside / "keep"} << "protected";
    fs::create_directory_symlink(outside, folder);
    CHECK_FALSE(registry.deleteWorld("temporary"));
    CHECK(fs::exists(outside / "keep"));
    fs::remove(folder);
    // Even a same-root alias of an active world is not a deletion target.
    fs::create_directory_symlink(fs::absolute(registry.defaultWorld().worldRoot()), folder);
    CHECK_FALSE(registry.deleteWorld("temporary"));
    fs::remove(folder);
    fs::rename(moved, folder);
    fs::create_directory_symlink(outside, folder / "external-link");
    REQUIRE(registry.deleteWorld("temporary"));
    CHECK(fs::exists(outside / "keep"));
    CHECK_FALSE(fs::exists(folder));
    CHECK(fs::exists(registry.defaultWorld().worldRoot()));
    CHECK_FALSE(registry.deleteWorld("temporary"));
    REQUIRE(registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 99) !=
            nullptr);
    CHECK(registry.worlds().at("temporary")->metadata().seed == 99);
}

TEST_CASE("generator factory statistics do not retain released generation bundles") {
    using Factory = basalt::server::runtime::world::VanillaWorldGeneratorFactory;
    Factory factory{
        {.readText = [](std::string_view) -> std::optional<std::string> { return std::nullopt; },
         .readEntry = [](std::string_view) -> std::optional<std::vector<std::uint8_t>> { return std::nullopt; },
         .dataIds = [](std::string_view,
                       std::string_view) { return std::vector<basalt::wire::common::Identifier>{}; }}};
    const config::WorldDeclaration declaration{.name = "flat", .type = config::WorldType::Flat};
    auto bundle = factory.create(declaration, {}, false);
    REQUIRE(bundle.lifetime != nullptr);
    std::weak_ptr<void> lifetime = bundle.lifetime;
    static_cast<void>(factory.cacheStats());
    CHECK_FALSE(lifetime.expired());
    bundle = {};
    CHECK(lifetime.expired());
    CHECK(factory.cacheStats().templates == 0);
}

TEST_CASE("login snapshots pin a live world only until player registration finishes") {
    const config::BasaltConfig config;
    const auto worlds = pairedWorlds();
    PlayerRegistry players;
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot = scratch("registry-login-leases")}};
    auto* world = registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 7);
    REQUIRE(world != nullptr);
    std::weak_ptr<LoadedWorld> weak = world->keepAlive();
    auto snapshot = registry.worldsForLogin();
    REQUIRE(snapshot.at("temporary").get() == world);
    CHECK_FALSE(registry.unloadWorld("temporary", false));
    snapshot.clear();
    REQUIRE(registry.unloadWorld("temporary", false));
    CHECK(weak.expired());
    CHECK_FALSE(registry.worldsForLogin().contains("temporary"));
}

TEST_CASE("tick snapshots survive unloads from global and region tasks without blocking unload") {
    const config::BasaltConfig config;
    const config::WorldsConfig worlds = pairedWorlds();
    PlayerRegistry players;
    WorldRegistry registry{config, worlds, nullptr, players, {}, {.worldsRoot = scratch("registry-tick-unload")}};
    REQUIRE(registry.createWorld("temporary", config::WorldDimension::Overworld, config::WorldType::Flat, 1));
    auto snapshot = registry.worldsForTick();
    std::weak_ptr<LoadedWorld> weak = snapshot.at("temporary");
    bool unloaded = false;
    const auto unload = [&] { unloaded = registry.unloadWorld("temporary", false); };
    SUBCASE("global tasks run after the world snapshot was taken") {
        registry.scheduler().global().schedule(0, unload);
    }
    SUBCASE("a region task unloads its own world") {
        registry.scheduler().regionFor("temporary").owner().schedule(0, unload);
    }
    int retiredTicks = 0;
    registry.scheduler().tick({}, [&](auto& region) {
        const auto found = snapshot.find(region.worldName());
        if (found == snapshot.end() || found->second->retired())
            return;
        auto& world = *found->second;
        region.owner().advanceTick();
        region.owner().runPending({});
        if (world.retired()) {
            ++retiredTicks;
            // This used to access a freed EntityRegistry mutex after unloading.
            CHECK(world.state().entities().empty());
        }
    });
    CHECK(unloaded);
    CHECK_FALSE(registry.worlds().contains("temporary"));
    CHECK(snapshot.at("temporary")->retired());
    CHECK_FALSE(weak.expired());
    CHECK(retiredTicks <= 1);
    snapshot.clear();
    CHECK(weak.expired());
}
