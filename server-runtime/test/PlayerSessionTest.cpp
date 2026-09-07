#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::storage::StoredItemStack;
using basalt::server::storage::StoredPlayer;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::Gamemode;
using basalt::wire::text::Component;
namespace persistence = basalt::server::runtime::player::playerPersistence;
namespace storage = basalt::server::storage;

namespace {

[[nodiscard]] PlayerConnection connectionOf(std::vector<Component>* sent = nullptr) {
    PlayerConnection connection;
    connection.protocolVersion = 776;
    connection.connectedHost = "play.basaltmc.org";
    connection.address = "203.0.113.7";
    if (sent != nullptr) {
        connection.sendSystemMessage = [sent](const Component& message) { sent->push_back(message); };
    }
    return connection;
}

[[nodiscard]] PlayerSession sessionOf(std::uint64_t id, const std::string& name,
                                      std::vector<Component>* sent = nullptr) {
    return PlayerSession{Uuid{id, id}, name, static_cast<std::int32_t>(id), connectionOf(sent)};
}

} // namespace

TEST_CASE("a fresh session is whole, fed and standing still") {
    const PlayerSession session = sessionOf(1, "Steve");

    CHECK(session.name() == "Steve");
    CHECK(session.entityId() == 1);
    CHECK(session.health() == doctest::Approx(PlayerSession::kMaxHealth));
    CHECK(session.maxHealth() == doctest::Approx(PlayerSession::kMaxHealth));
    CHECK(session.food.foodLevel() == 20);
    CHECK(session.airSupply == 300);
    CHECK(session.skinParts == PlayerSession::kAllSkinParts);
    CHECK(session.gamemode == Gamemode::Survival);
    CHECK_FALSE(session.dead);
}

TEST_CASE("the facts the connection carries are read straight off it") {
    const PlayerSession session = sessionOf(2, "Alex");

    CHECK(session.protocolVersion() == 776);
    CHECK(session.connectedHost() == "play.basaltmc.org");
    REQUIRE(session.address().has_value());
    CHECK(*session.address() == "203.0.113.7");
}

TEST_CASE("moving the ceiling down brings the health with it, and it never reaches zero") {
    PlayerSession session = sessionOf(3, "Steve");

    session.setMaxHealth(6.0F);
    CHECK(session.maxHealth() == doctest::Approx(6.0F));
    CHECK(session.health() == doctest::Approx(6.0F));

    // Raising it again leaves the health where it was: a bigger bar is not a heal.
    session.setMaxHealth(20.0F);
    CHECK(session.health() == doctest::Approx(6.0F));

    session.setMaxHealth(-5.0F);
    CHECK(session.maxHealth() == doctest::Approx(1.0F));
    CHECK(session.health() == doctest::Approx(1.0F));
}

TEST_CASE("what a game mode allows follows the mode and nothing else") {
    PlayerSession session = sessionOf(4, "Steve");
    CHECK_FALSE(session.mayFly());
    CHECK_FALSE(session.invulnerable());
    CHECK_FALSE(session.instabuild());

    session.gamemode = Gamemode::Creative;
    CHECK(session.mayFly());
    CHECK(session.invulnerable());
    CHECK(session.instabuild());

    session.gamemode = Gamemode::Spectator;
    CHECK(session.mayFly());
    CHECK(session.invulnerable());
    CHECK_FALSE(session.instabuild());

    session.gamemode = Gamemode::Adventure;
    CHECK_FALSE(session.mayFly());
    CHECK_FALSE(session.invulnerable());
}

TEST_CASE("a message reaches the connection, and a session without one is not an error") {
    std::vector<Component> sent;
    const PlayerSession heard = sessionOf(5, "Steve", &sent);

    heard.sendSystemMessage(Component::text("hello"));

    REQUIRE(sent.size() == 1);
    CHECK(sent[0] == Component::text("hello"));

    const PlayerSession alone = sessionOf(6, "Alex");
    alone.sendSystemMessage(Component::text("nobody hears this"));
}

TEST_CASE("a respawn puts back everything a death took, and keeps what it does not") {
    PlayerSession session = sessionOf(7, "Steve");
    session.setHealth(0.0F);
    session.dead = true;
    session.food.restore(2, 0.0F, 3.0F);
    session.experience.givePoints(100);
    session.sprinting = true;
    session.vehicleId = 44;
    session.airSupply = 0;
    session.fireTicks = 120;
    session.effects.emplace(
        basalt::wire::common::Identifier::vanilla("speed"),
        basalt::server::runtime::world::ActiveEffect{
            .effect = basalt::wire::common::Identifier::vanilla("speed"), .amplifier = 0, .remainingTicks = 200});
    session.inventory.set(0, StoredItemStack{.id = "minecraft:stone", .count = 1, .components = {}});

    session.resetForRespawn();

    CHECK(session.health() == doctest::Approx(PlayerSession::kMaxHealth));
    CHECK(session.food.foodLevel() == 20);
    CHECK(session.experience.level() == 0);
    CHECK(session.experience.total() == 0);
    CHECK_FALSE(session.dead);
    CHECK_FALSE(session.sprinting);
    CHECK_FALSE(session.vehicleId.has_value());
    CHECK(session.airSupply == 300);
    CHECK(session.fireTicks == 0);
    CHECK(session.effects.empty());
    // parity: the inventory is not this method's business; keeping it is `keepInventory`'s.
    CHECK(session.inventory.at(0).has_value());
}

TEST_CASE("a spectator comes back flying and everyone else on their feet") {
    PlayerSession spectator = sessionOf(8, "Steve");
    spectator.gamemode = Gamemode::Spectator;
    spectator.resetForRespawn();
    CHECK(spectator.flying);

    PlayerSession survivor = sessionOf(9, "Alex");
    survivor.flying = true;
    survivor.resetForRespawn();
    CHECK_FALSE(survivor.flying);
}

TEST_CASE("a saved row carries everything a rejoin has to put back") {
    PlayerSession session = sessionOf(10, "Steve");
    session.x = 1.5;
    session.y = 64.0;
    session.z = -2.5;
    session.yaw = 90.0F;
    session.pitch = -10.0F;
    session.setHealth(7.5F);
    session.food.restore(11, 2.0F, 1.5F);
    session.experience.restore(4, 0.25F, 90);
    session.gamemode = Gamemode::Adventure;
    session.group = "builders";
    session.tags.emplace("chosen");
    session.stats.increment(storage::stats::jump(), 3);
    session.inventory.set(2, StoredItemStack{.id = "minecraft:stone", .count = 9, .components = {}});
    session.enderChest[1] = StoredItemStack{.id = "minecraft:diamond", .count = 1, .components = {}};

    const StoredPlayer stored = persistence::toStored(session, "overworld", 1000, 5000, 9000);

    CHECK(stored.uuid == session.uuid().toString());
    CHECK(stored.name == "Steve");
    CHECK(stored.world == "overworld");
    CHECK(stored.firstSeenEpochSeconds == 1000);
    CHECK(stored.lastSeenEpochSeconds == 9000);
    CHECK(stored.playTimeTicks == 5000);
    CHECK(stored.x == doctest::Approx(1.5));
    CHECK(stored.health == doctest::Approx(7.5F));
    CHECK(stored.food == 11);
    CHECK(stored.xpLevel == 4);
    CHECK(stored.gamemode == storage::StoredGamemode::Adventure);
    REQUIRE(stored.group.has_value());
    CHECK(*stored.group == "builders");
    CHECK(stored.tags.contains("chosen"));
    CHECK(stored.stats.get(storage::stats::jump()) == 3);
    CHECK(stored.stats.get(storage::stats::playTime()) == 5000);
    REQUIRE(stored.inventory.slots.size() > 2);
    REQUIRE(stored.inventory.slots[2].has_value());
    CHECK(stored.inventory.slots[2]->count == 9);
    REQUIRE(stored.enderChest.slots[1].has_value());
    CHECK(stored.enderChest.slots[1]->id == "minecraft:diamond");
}

TEST_CASE("a rejoin is the row put back on a fresh session") {
    PlayerSession first = sessionOf(11, "Steve");
    first.x = 3.5;
    first.y = 70.0;
    first.z = 8.5;
    first.setHealth(4.0F);
    first.food.restore(7, 1.0F, 2.0F);
    first.experience.restore(9, 0.5F, 300);
    first.gamemode = Gamemode::Spectator;
    first.group = "admins";
    first.tags.emplace("veteran");
    first.stats.increment(storage::stats::deaths(), 2);
    first.inventory.set(4, StoredItemStack{.id = "minecraft:torch", .count = 5, .components = {}});
    first.enderChest[0] = StoredItemStack{.id = "minecraft:emerald", .count = 2, .components = {}};

    const StoredPlayer stored = persistence::toStored(first, "overworld", 1, 2, 3);

    PlayerSession second = sessionOf(11, "Steve");
    second.tags.emplace("stale");
    persistence::restore(second, stored);

    CHECK(second.x == doctest::Approx(3.5));
    CHECK(second.health() == doctest::Approx(4.0F));
    CHECK(second.food.foodLevel() == 7);
    CHECK(second.food.exhaustionLevel() == doctest::Approx(2.0F));
    CHECK(second.experience.level() == 9);
    CHECK(second.experience.total() == 300);
    CHECK(second.gamemode == Gamemode::Spectator);
    // parity: a spectator is always flying, whatever the row said.
    CHECK(second.flying);
    REQUIRE(second.group.has_value());
    CHECK(*second.group == "admins");
    CHECK(second.stats.get(storage::stats::deaths()) == 2);
    // The play time has its own column, so it is not counted twice.
    CHECK(second.stats.get(storage::stats::playTime()) == 0);
    REQUIRE(second.inventory.at(4).has_value());
    CHECK(second.inventory.at(4)->count == 5);
    REQUIRE(second.enderChest[0].has_value());
    CHECK(second.enderChest[0]->id == "minecraft:emerald");
    CHECK(second.tags.size() == 1);
    CHECK(second.tags.contains("veteran"));
}

TEST_CASE("a row with a shorter ender chest leaves the rest of it empty") {
    PlayerSession session = sessionOf(12, "Steve");
    session.enderChest[3] = StoredItemStack{.id = "minecraft:stone", .count = 1, .components = {}};
    StoredPlayer stored;
    stored.enderChest.slots.clear();

    persistence::restore(session, stored);

    CHECK_FALSE(session.enderChest[3].has_value());
}

TEST_CASE("the registry hands out one entity id at a time") {
    PlayerRegistry registry;

    CHECK(registry.allocateEntityId() == 1);
    CHECK(registry.allocateEntityId() == 2);
    CHECK(registry.allocateEntityId() == 3);
}

TEST_CASE("the registry finds a player by their uuid and forgets them on request") {
    PlayerRegistry registry;
    PlayerSession steve = sessionOf(20, "Steve");
    PlayerSession alex = sessionOf(21, "Alex");

    registry.add(&steve);
    registry.add(&alex);

    CHECK(registry.get(steve.uuid()) == &steve);
    CHECK(registry.get(alex.uuid()) == &alex);
    CHECK(registry.get(Uuid{99, 99}) == nullptr);
    CHECK(registry.snapshot().size() == 2);

    registry.remove(steve.uuid());
    CHECK(registry.get(steve.uuid()) == nullptr);
    REQUIRE(registry.snapshot().size() == 1);
    CHECK(registry.snapshot()[0] == &alex);

    // Forgetting somebody who is not there is not an error.
    registry.remove(Uuid{99, 99});
    CHECK(registry.snapshot().size() == 1);
}

TEST_CASE("the registry lists players in the order they joined") {
    PlayerRegistry registry;
    PlayerSession first = sessionOf(30, "First");
    PlayerSession second = sessionOf(31, "Second");
    PlayerSession third = sessionOf(32, "Third");

    registry.add(&first);
    registry.add(&second);
    registry.add(&third);

    const std::vector<PlayerSession*> listed = registry.snapshot();
    REQUIRE(listed.size() == 3);
    CHECK(listed[0] == &first);
    CHECK(listed[1] == &second);
    CHECK(listed[2] == &third);
}

TEST_CASE("the same player joining again replaces the session that was there") {
    PlayerRegistry registry;
    PlayerSession before = sessionOf(40, "Steve");
    PlayerSession after = sessionOf(40, "Steve");

    registry.add(&before);
    registry.add(&after);

    CHECK(registry.get(after.uuid()) == &after);
    REQUIRE(registry.snapshot().size() == 1);
    CHECK(registry.snapshot()[0] == &after);
}

TEST_CASE("adding nobody at all does nothing") {
    PlayerRegistry registry;

    registry.add(nullptr);

    CHECK(registry.snapshot().empty());
}
