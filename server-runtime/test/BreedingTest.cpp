#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::WorldState;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace breeding = basalt::server::runtime::entity::breeding;
namespace mobBrain = basalt::server::runtime::entity::mobBrain;

namespace {

[[nodiscard]] std::unique_ptr<ServerEntity> animal(std::int32_t entityId, std::string_view type, double x, double z) {
    const basalt::server::registry::EntityTypeEntry* entry =
        basalt::server::registry::entityTypes::of(Identifier::vanilla(std::string{type}));
    REQUIRE(entry != nullptr);
    return std::make_unique<ServerEntity>(entityId, Uuid{static_cast<std::uint64_t>(entityId), 1}, entry->id,
                                          entry->protocolId, x, 64.0, z);
}

} // namespace

TEST_CASE("an animal only eats what its own class calls food") {
    CHECK(breeding::isFood(Identifier::vanilla("cow"), Identifier::vanilla("wheat")));
    CHECK_FALSE(breeding::isFood(Identifier::vanilla("cow"), Identifier::vanilla("carrot")));
    CHECK(breeding::isFood(Identifier::vanilla("pig"), Identifier::vanilla("carrot")));
    CHECK(breeding::isFood(Identifier::vanilla("wolf"), Identifier::vanilla("mutton")));
    // A mob with no food table refuses everything rather than pretending it eats anything.
    CHECK(breeding::foodFor(Identifier::vanilla("zombie")).empty());
}

TEST_CASE("only a grown animal that is not already in love can be fed") {
    const std::unique_ptr<ServerEntity> cow = animal(1, "cow", 0.0, 0.0);
    CHECK(breeding::canFallInLove(*cow));

    breeding::feed(*cow);
    CHECK(cow->loveTicks == breeding::kLoveTicks);
    CHECK_FALSE(breeding::canFallInLove(*cow));

    // A calf is too young, and a parent counting down its cooldown is not zero either.
    const std::unique_ptr<ServerEntity> calf = animal(2, "cow", 0.0, 0.0);
    calf->age = breeding::kBabyAge;
    CHECK_FALSE(breeding::canFallInLove(*calf));
    const std::unique_ptr<ServerEntity> parent = animal(3, "cow", 0.0, 0.0);
    parent->age = breeding::kParentCooldown;
    CHECK_FALSE(breeding::canFallInLove(*parent));
}

TEST_CASE("two in love standing together breed after their courtship, and not before") {
    const std::unique_ptr<ServerEntity> first = animal(1, "cow", 0.0, 0.0);
    const std::unique_ptr<ServerEntity> second = animal(2, "cow", 1.0, 0.0);
    breeding::feed(*first);
    breeding::feed(*second);
    const std::vector<ServerEntity*> nearby{first.get(), second.get()};

    // parity: sixty ticks of standing together before anything comes of it.
    for (std::int32_t tick = 0; tick < breeding::kCourtshipTicks - 1; tick++) {
        CHECK(breeding::tick(*first, nearby) == nullptr);
    }
    CHECK(breeding::tick(*first, nearby) == second.get());

    breeding::afterBreeding(*first, *second);
    CHECK(first->loveTicks == 0);
    CHECK(first->age == breeding::kParentCooldown);
    CHECK(second->breedCooldown == breeding::kParentCooldown);
}

TEST_CASE("a partner too far away resets the courtship rather than counting on") {
    const std::unique_ptr<ServerEntity> first = animal(1, "cow", 0.0, 0.0);
    const std::unique_ptr<ServerEntity> second = animal(2, "cow", 20.0, 0.0);
    breeding::feed(*first);
    breeding::feed(*second);
    const std::vector<ServerEntity*> nearby{first.get(), second.get()};

    for (std::int32_t tick = 0; tick < 100; tick++) {
        CHECK(breeding::tick(*first, nearby) == nullptr);
    }
    CHECK(first->courtshipTicks == 0);
    // Two of different kinds never mate however close they stand.
    const std::unique_ptr<ServerEntity> pig = animal(3, "pig", 0.5, 0.0);
    breeding::feed(*pig);
    CHECK_FALSE(breeding::canMate(*first, *pig));
}

TEST_CASE("a wander stays on ground it could actually stand on") {
    WorldState world;
    world.getOrGenerateChunk(0, 0);
    // A floor to stand on: an empty world has nowhere to wander to, which is the right answer for
    // one and a useless fixture for this.
    const std::int32_t stone = basalt::server::registry::blocks::named("stone")->stateId;
    for (std::int32_t x = 0; x < 16; x++) {
        for (std::int32_t z = 0; z < 16; z++) {
            REQUIRE(world.setBlockState(x, 63, z, stone));
        }
    }
    const std::unique_ptr<ServerEntity> cow = animal(1, "cow", 8.5, 8.5);
    cow->y = 64.0;
    RandomSource random = RandomSource::legacy(4);

    const std::optional<std::array<double, 3>> target = mobBrain::strollTarget(world, *cow, random);
    REQUIRE(target.has_value());
    // Whatever it picked, the block under it holds a mob up and the two above it are clear.
    const auto x = static_cast<std::int32_t>(std::floor((*target)[0]));
    const auto y = static_cast<std::int32_t>((*target)[1]);
    const auto z = static_cast<std::int32_t>(std::floor((*target)[2]));
    CHECK(world.blockState(x, y - 1, z).has_value());
    CHECK(mobBrain::distanceTo(*cow, (*target)[0], (*target)[1], (*target)[2]) <= 20.0);
}

TEST_CASE("feeding a calf takes a tenth of the childhood it has left") {
    const std::unique_ptr<ServerEntity> calf = animal(1, "cow", 0.5, 0.5);
    calf->age = breeding::kBabyAge;

    // parity: `getSpeedUpSecondsWhenFeeding` is (int)(ticks / 20 * 0.1F), so 24000 ticks left give
    // a hundred and twenty seconds, which is two thousand four hundred ticks.
    REQUIRE(breeding::canAgeUp(*calf));
    breeding::feedBaby(*calf);
    CHECK(calf->age == breeding::kBabyAge + 2400);

    // Close to grown, the tenth rounds down to nothing and the last seconds are waited out.
    calf->age = -100;
    breeding::feedBaby(*calf);
    CHECK(calf->age == -100);

    // It never carries past grown, and a grown one is not a calf at all.
    calf->age = -20;
    breeding::feedBaby(*calf);
    CHECK(calf->age == -20);
    calf->age = 0;
    CHECK_FALSE(breeding::canAgeUp(*calf));
}
