#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>

#include <doctest.h>

#include <cstdint>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace rockets = basalt::server::runtime::entity::fireworkRockets;

namespace {

ServerEntity rocketEntity() {
    const basalt::server::registry::EntityTypeEntry* entry =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("firework_rocket"));
    REQUIRE(entry != nullptr);
    return ServerEntity{1, Uuid{1, 1}, entry->id, entry->protocolId, 0.0, 64.0, 0.0};
}

} // namespace

TEST_CASE("a celebration rocket is one burst in the dye's firework colour, and flies for its flight") {
    const basalt::wire::nbt::NbtCompound item = rockets::celebrationRocket(14, 2);
    CHECK(rockets::flightDuration(item) == 2);
    CHECK(rockets::explosionCount(item) == 1);
    // parity: the fireworks component reaches the client, stars and all.
    const basalt::wire::nbt::NbtCompound* components = item.getCompound("components");
    REQUIRE(components != nullptr);
    CHECK(basalt::server::runtime::item::itemComponentCodecs::toWire(*components, {}).size() == 1);

    ServerEntity rocket = rocketEntity();
    RandomSource random = RandomSource::legacy(3);
    const rockets::RocketState& state = rockets::start(rocket, item, random);
    // parity: ten ticks a flight level, three of them here, plus up to five and up to six more.
    CHECK(state.lifetime >= 30);
    CHECK(state.lifetime <= 41);
    CHECK(rocket.velocityY == 0.05);
}

TEST_CASE("a rocket keeps its life, lifetime, item and angle across a save") {
    ServerEntity rocket = rocketEntity();
    RandomSource random = RandomSource::legacy(9);
    rockets::start(rocket, rockets::celebrationRocket(3, 1), random).shotAtAngle = true;
    rocket.firework->life = 7;
    basalt::wire::nbt::NbtCompound tag;
    rockets::write(rocket, tag);

    ServerEntity reloaded = rocketEntity();
    rockets::read(tag, reloaded);
    REQUIRE(reloaded.firework != nullptr);
    CHECK(reloaded.firework->life == 7);
    CHECK(reloaded.firework->lifetime == rocket.firework->lifetime);
    CHECK(reloaded.firework->shotAtAngle);
    CHECK(rockets::explosionCount(reloaded.firework->item) == 1);
}
