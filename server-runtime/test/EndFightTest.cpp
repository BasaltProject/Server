#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/AreaEffectCloudSystem.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/EndGateways.hpp>
#include <basalt/server/runtime/world/EndPodium.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <doctest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::AreaEffectCloudSystem;
using basalt::server::runtime::BlockChangeBroadcaster;
using basalt::server::runtime::CombatSystem;
using basalt::server::runtime::EndFightSystem;
using basalt::server::runtime::ExplosionSystem;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::RealLootTables;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace config = basalt::server::runtime::config;
namespace world = basalt::server::runtime::world;
namespace dragons = basalt::server::runtime::entity::enderDragon;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

struct Arena {
    PlayerRegistry registry;
    std::unique_ptr<LoadedWorld> end;
    std::unique_ptr<PlayerSession> player;
    RealLootTables tables{
        RealLootTables::Jar{.readText = [](std::string_view) { return std::optional<std::string>{}; },
                            .entriesUnder = [](std::string_view) { return std::vector<std::string>{}; }}};
    BlockChangeBroadcaster broadcaster{registry, tables, {}, {.nextSeed = [] { return std::int64_t{3}; }}};
    ExplosionSystem explosions{registry, tables, {}, {.nextSeed = [] { return std::int64_t{3}; }}};
    CombatSystem combat{broadcaster, explosions, {}, {.nextInt = [](std::int32_t) { return 0; }}};
    AreaEffectCloudSystem clouds{registry, combat, {}};
    EndFightSystem fight{registry,   broadcaster, combat,
                         explosions, clouds,      {.nextSeed = [] { return std::int64_t{5}; }}};

    explicit Arena(const std::string& root)
        : end(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{
                  .name = "the_end", .dimension = config::WorldDimension::End, .type = config::WorldType::Void},
              config::WorldSettings{}, WorldMetadata{.seed = 1, .spawnX = 8, .spawnY = 64, .spawnZ = 8}, root,
              basalt::wire::common::Logger{"Test"})) {
        player = std::make_unique<PlayerSession>(Uuid{1, 1}, "Steve", 1, PlayerConnection{});
        player->worldName = "the_end";
        player->x = 10.0;
        player->y = 100.0;
        player->z = 10.0;
        registry.add(player.get());
    }

    [[nodiscard]] std::vector<ServerEntity*> living() {
        std::vector<ServerEntity*> alive;
        for (ServerEntity* entity : end->state().entities()) {
            if (!entity->removed) {
                alive.push_back(entity);
            }
        }
        return alive;
    }

    void tick() {
        fight.tickFight(*end, {player.get()}, living());
        for (ServerEntity* entity : living()) {
            if (dragons::isDragon(entity->type())) {
                fight.tickDragon(*end, {player.get()}, living(), *entity);
            }
        }
    }

    void crystalAt(double x, double y, double z) {
        const auto* entry = basalt::server::registry::entityTypes::of(Identifier::vanilla("end_crystal"));
        REQUIRE(entry != nullptr);
        static_cast<void>(end->state().spawnEntity(
            std::make_unique<ServerEntity>(static_cast<std::int32_t>(end->state().entities().size() + 100),
                                           Uuid::random(), entry->id, entry->protocolId, x, y, z)));
    }

    [[nodiscard]] std::int32_t dragons() {
        std::int32_t count = 0;
        for (const ServerEntity* entity : living()) {
            count += dragons::isDragon(entity->type()) ? 1 : 0;
        }
        return count;
    }
};

} // namespace

TEST_CASE("a fresh end calls up its dragon, and four crystals by the portal call a dead one back") {
    Arena arena{scratch("end-fight")};
    world::DragonFight* fight = arena.end->dragonFight();
    REQUIRE(fight != nullptr);
    static_cast<void>(world::endPodium::ensure(arena.end->state(), false));

    // parity: twenty ticks to notice the player, then the scan and a dragon at the centre.
    for (std::int32_t tick = 0; tick < 25; ++tick) {
        arena.tick();
    }
    CHECK_FALSE(fight->needsStateScanning);
    CHECK(fight->dragonUuid.has_value());
    CHECK(arena.dragons() == 1);

    // Kill it the long way round: the death animation ends in the lit portal.
    for (ServerEntity* entity : arena.living()) {
        if (dragons::isDragon(entity->type())) {
            entity->health = 0.0;
        }
    }
    for (std::int32_t tick = 0; tick < 205; ++tick) {
        arena.tick();
    }
    REQUIRE(fight->dragonKilled);
    CHECK(fight->previouslyKilled);
    CHECK(arena.dragons() == 0);
    REQUIRE(fight->exitPortalLocation.has_value());
    const std::int32_t origin = (*fight->exitPortalLocation)[1];
    CHECK(world::endPodium::isActive(arena.end->state(), origin));
    CHECK(fight->gateways.size() == 19);

    // Three crystals are not enough.
    arena.crystalAt(3.5, origin + 1.0, 0.5);
    arena.crystalAt(-2.5, origin + 1.0, 0.5);
    arena.crystalAt(0.5, origin + 1.0, 3.5);
    arena.fight.tryRespawn(*arena.end);
    CHECK_FALSE(fight->respawnStage.has_value());

    arena.crystalAt(0.5, origin + 1.0, -2.5);
    arena.fight.tryRespawn(*arena.end);
    REQUIRE(fight->respawnStage == world::RespawnStage::Start);
    CHECK(fight->respawnCrystals.size() == 4);
    CHECK_FALSE(world::endPodium::isActive(arena.end->state(), origin));

    // parity: one tick to start, a hundred preparing, forty per pillar, a hundred summoning.
    std::int32_t ticks = 0;
    while (fight->respawnStage.has_value() && ticks < 800) {
        arena.tick();
        ++ticks;
    }
    CHECK_FALSE(fight->respawnStage.has_value());
    CHECK_FALSE(fight->dragonKilled);
    CHECK(arena.dragons() == 1);
    CHECK(ticks > 600);
    // The four crystals went off, and the pillars' new ones are no longer shielded.
    std::int32_t shielded = 0;
    std::int32_t crystals = 0;
    for (const ServerEntity* entity : arena.living()) {
        if (world::endCrystals::isCrystal(entity->type())) {
            ++crystals;
            shielded += world::endCrystals::isInvulnerable(*entity) ? 1 : 0;
        }
    }
    CHECK(crystals == 10);
    CHECK(shielded == 0);
}

TEST_CASE("a gateway with no exit builds an island and a way back, and then always leads there") {
    Arena arena{scratch("end-gateway-exit")};
    world::WorldState& state = arena.end->state();
    world::endGateways::place(state, 100, 75, 0, std::nullopt, false);

    const std::optional<std::array<double, 3>> first = world::endGateways::destination(state, 100, 75, 0, true);
    REQUIRE(first.has_value());
    // parity: an empty End pushes the search sixteen chunks past the thousand blocks, so the
    // island lands at x 1280 and the traveller on top of it.
    CHECK((*first)[0] > 1260.0);
    CHECK((*first)[0] < 1300.0);
    CHECK((*first)[1] == doctest::Approx(76.0));

    // The far gateway stands ten above the island and leads back to the entry.
    const std::int32_t farX = static_cast<std::int32_t>(std::floor((*first)[0]));
    const std::int32_t farZ = static_cast<std::int32_t>(std::floor((*first)[2]));
    bool found = false;
    for (std::int32_t dx = -8; dx <= 8 && !found; ++dx) {
        for (std::int32_t dz = -8; dz <= 8 && !found; ++dz) {
            const std::optional<std::int32_t> block = state.blockState(farX + dx, 85, farZ + dz);
            if (block.has_value() && world::endGateways::isGateway(*block)) {
                found = true;
                const std::optional<std::array<double, 3>> back =
                    world::endGateways::destination(state, farX + dx, 85, farZ + dz, true);
                REQUIRE(back.has_value());
                CHECK(std::abs((*back)[0] - 100.5) < 6.0);
                CHECK(std::abs((*back)[2] - 0.5) < 6.0);
            }
        }
    }
    CHECK(found);

    const std::optional<std::array<double, 3>> again = world::endGateways::destination(state, 100, 75, 0, true);
    REQUIRE(again.has_value());
    CHECK(*again == *first);
    // Outside the End a gateway without an exit leads nowhere.
    world::endGateways::place(state, -100, 75, 0, std::nullopt, false);
    CHECK_FALSE(world::endGateways::destination(state, -100, 75, 0, false).has_value());
}
