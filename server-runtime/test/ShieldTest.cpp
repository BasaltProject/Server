#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <doctest.h>

#include <cstdint>
#include <optional>

using basalt::server::runtime::entity::DamageSource;
using basalt::server::runtime::player::DamageContext;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerInventory;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;

namespace {

const std::optional<Identifier> kMobAttack = Identifier::vanilla("mob_attack");

/// A player facing south with a shield up in the off hand for long enough to block.
struct Guard {
    PlayerRegistry registry;
    PlayerSession steve{Uuid{1, 1}, "Steve", 1, PlayerConnection{}};
    DamageContext context;

    Guard() {
        registry.add(&steve);
        steve.inventory.set(PlayerInventory::kOffhandSlot,
                            StoredItemStack{.id = "minecraft:shield", .count = 1, .components = {}});
        steve.blockingSlot = PlayerInventory::kOffhandSlot;
        steve.blockingTicks = 5;
        context.registry = &registry;
    }

    void hit(float amount, double fromX, double fromZ, std::int32_t attacker = 99) {
        steve.invulnerableTicks = 0;
        basalt::server::runtime::player::hurtPlayer(
            steve, amount, "mob", kMobAttack, DamageSource{.entityId = attacker, .x = fromX, .z = fromZ}, context);
    }
};

} // namespace

TEST_CASE("a shield takes a hit from in front and none from behind") {
    Guard guard;
    guard.hit(5.0F, 0.0, 5.0);
    CHECK(guard.steve.health() == doctest::Approx(20.0F));
    // parity: `ItemDamageFunction`, one plus the damage once it reaches three.
    CHECK(guard.steve.inventory.at(PlayerInventory::kOffhandSlot)->components.getInt("minecraft:damage") == 6);

    guard.hit(5.0F, 0.0, -5.0);
    CHECK(guard.steve.health() == doctest::Approx(15.0F));
}

TEST_CASE("a shield only blocks once it has been up for a quarter second") {
    Guard guard;
    guard.steve.blockingTicks = 4;
    guard.hit(4.0F, 0.0, 5.0);
    CHECK(guard.steve.health() == doctest::Approx(16.0F));
}

TEST_CASE("an axe knocks the shield away for five seconds") {
    Guard guard;
    PlayerSession alex{Uuid{2, 2}, "Alex", 2, PlayerConnection{}};
    alex.inventory.set(PlayerInventory::kHotbarStart,
                       StoredItemStack{.id = "minecraft:iron_axe", .count = 1, .components = {}});
    guard.registry.add(&alex);
    std::int32_t disabledFor = 0;
    guard.context.disableBlocking = [&disabledFor](PlayerSession& player, std::int32_t ticks) {
        disabledFor = ticks;
        player.blockingSlot.reset();
    };
    guard.hit(6.0F, 0.0, 2.0, alex.entityId());
    CHECK(guard.steve.health() == doctest::Approx(20.0F));
    CHECK(disabledFor == 100);
    CHECK_FALSE(guard.steve.blockingSlot.has_value());
    guard.registry.remove(alex.uuid());
}
