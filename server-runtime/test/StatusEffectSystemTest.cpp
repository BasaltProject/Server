#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/StatusEffectSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::StatusEffectSystem;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::ActiveEffect;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace config = basalt::server::runtime::config;
namespace mobEffects = basalt::server::runtime::world::mobEffects;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

/// One player in one world, with every effect hook recorded rather than sent.
struct Fixture {
    PlayerRegistry registry;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<PlayerSession> player;
    float healed = 0.0F;
    std::vector<Identifier> hurtBy;
    std::vector<Identifier> removed;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})) {
        PlayerConnection connection;
        connection.heal = [this](float amount) { healed += amount; };
        connection.hurt = [this](float, std::string_view, const Identifier& type,
                               const std::optional<basalt::server::runtime::entity::DamageSource>&) { hurtBy.push_back(type); };
        connection.sendEffectRemoved = [this](const Identifier& effect) { removed.push_back(effect); };
        player = std::make_unique<PlayerSession>(Uuid{1, 1}, "Steve", 1, std::move(connection));
        player->worldName = "world";
        registry.add(player.get());
    }

    void give(const Identifier& effect, std::int32_t amplifier, std::int32_t ticks) {
        player->effects.insert_or_assign(
            effect, ActiveEffect{.effect = effect, .amplifier = amplifier, .remainingTicks = ticks});
    }
};

} // namespace

TEST_CASE("regeneration heals on its own interval, which halves with every level") {
    Fixture fixture{scratch("effects-regeneration")};
    StatusEffectSystem system{fixture.registry, {}};
    fixture.player->setHealth(1.0F);

    // 50 ticks between heals at level 0, so a countdown landing on a multiple of 50 heals.
    fixture.give(mobEffects::regeneration(), 0, 100);
    system.tickEffects(*fixture.world);
    CHECK(fixture.healed == 1.0F);
    system.tickEffects(*fixture.world);
    CHECK(fixture.healed == 1.0F);

    // Level 1 halves it to 25, so the same 99th tick that did nothing before now heals.
    fixture.healed = 0.0F;
    fixture.give(mobEffects::regeneration(), 1, 100);
    system.tickEffects(*fixture.world);
    CHECK(fixture.healed == 1.0F);

    // A player already at full health is not healed at all.
    fixture.healed = 0.0F;
    fixture.player->setHealth(fixture.player->maxHealth());
    fixture.give(mobEffects::regeneration(), 0, 100);
    system.tickEffects(*fixture.world);
    CHECK(fixture.healed == 0.0F);
}

TEST_CASE("poison stops at one point of health and wither does not") {
    Fixture fixture{scratch("effects-poison")};
    StatusEffectSystem system{fixture.registry, {}};

    fixture.player->setHealth(1.0F);
    fixture.give(mobEffects::poison(), 0, 50);
    system.tickEffects(*fixture.world);
    CHECK(fixture.hurtBy.empty());

    fixture.player->effects.clear();
    fixture.give(mobEffects::wither(), 0, 40);
    system.tickEffects(*fixture.world);
    CHECK(fixture.hurtBy.size() == 1);
}

TEST_CASE("an effect that runs out is removed and the client told, and an infinite one never does") {
    Fixture fixture{scratch("effects-expiry")};
    StatusEffectSystem system{fixture.registry, {}};

    fixture.give(mobEffects::hunger(), 0, 1);
    system.tickEffects(*fixture.world);
    CHECK(fixture.player->effects.empty());
    CHECK(fixture.removed == std::vector<Identifier>{mobEffects::hunger()});

    // parity: -1 ticks is the infinite one, and it neither counts down nor expires.
    fixture.removed.clear();
    fixture.give(mobEffects::hunger(), 0, -1);
    system.tickEffects(*fixture.world);
    system.tickEffects(*fixture.world);
    CHECK(fixture.player->effects.at(mobEffects::hunger()).remainingTicks == -1);
    CHECK(fixture.removed.empty());
}

TEST_CASE("the omens are handed to whoever runs raids rather than ticked here") {
    Fixture fixture{scratch("effects-omens")};
    std::vector<Identifier> omens;
    StatusEffectSystem system{fixture.registry, [&omens](LoadedWorld&, PlayerSession&, const ActiveEffect& active) {
                                  omens.push_back(active.effect);
                              }};

    fixture.give(mobEffects::badOmen(), 0, 100);
    fixture.give(mobEffects::raidOmen(), 0, 100);
    system.tickEffects(*fixture.world);
    CHECK(omens.size() == 2);

    // A dead player is not ticked at all, which is what stops a raid starting over a corpse.
    omens.clear();
    fixture.player->dead = true;
    system.tickEffects(*fixture.world);
    CHECK(omens.empty());
}
