#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

#include <string>

using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace breathing = basalt::server::runtime::entity::mobBreathing;

namespace {

[[nodiscard]] ServerEntity mobOf(const std::string& path) {
    return ServerEntity{1, Uuid{1, 1}, Identifier::vanilla(path), 0, 0.0, 64.0, 0.0};
}

const breathing::Surroundings kUnderwater{.eyeInWater = true, .inWater = true};
const breathing::Surroundings kDry{};

} // namespace

TEST_CASE("a cow drowns twenty ticks after its air runs out, two points at a time") {
    ServerEntity cow = mobOf("cow");
    CHECK(cow.airSupply == 300);
    std::int32_t hits = 0;
    for (std::int32_t tick = 0; tick < 320; ++tick) {
        for (const breathing::Hurt& hurt : breathing::tick(cow, kUnderwater)) {
            CHECK(hurt.amount == doctest::Approx(2.0F));
            CHECK(hurt.cause == "drown");
            ++hits;
        }
    }
    CHECK(hits == 1);
    CHECK(cow.airSupply == 0);
    // parity: four ticks of air back for each one above.
    static_cast<void>(breathing::tick(cow, kDry));
    CHECK(cow.airSupply == 4);
}

TEST_CASE("the undead and the bubble column never run short") {
    ServerEntity zombie = mobOf("zombie");
    for (std::int32_t tick = 0; tick < 400; ++tick) {
        CHECK(breathing::tick(zombie, kUnderwater).empty());
    }
    CHECK(zombie.airSupply == 300);
    ServerEntity cow = mobOf("cow");
    static_cast<void>(breathing::tick(cow, {.eyeInWater = true, .eyeInBubbles = true, .inWater = true}));
    CHECK(cow.airSupply == 300);
}

TEST_CASE("a fish chokes on land and an axolotl dries out unless it rains") {
    ServerEntity cod = mobOf("cod");
    std::int32_t hits = 0;
    for (std::int32_t tick = 0; tick < 320; ++tick) {
        hits += static_cast<std::int32_t>(breathing::tick(cod, kDry).size());
    }
    CHECK(hits == 1);
    static_cast<void>(breathing::tick(cod, kUnderwater));
    CHECK(cod.airSupply == 300);

    ServerEntity axolotl = mobOf("axolotl");
    CHECK(axolotl.airSupply == 6000);
    static_cast<void>(breathing::tick(axolotl, {.inRain = true}));
    CHECK(axolotl.airSupply == 6000);
    axolotl.airSupply = -19;
    const std::vector<breathing::Hurt> owed = breathing::tick(axolotl, kDry);
    REQUIRE(owed.size() == 1);
    CHECK(owed.front().cause == "dry_out");
}

TEST_CASE("a dolphin holds four minutes and fills up in one breath") {
    ServerEntity dolphin = mobOf("dolphin");
    CHECK(dolphin.airSupply == 4800);
    for (std::int32_t tick = 0; tick < 100; ++tick) {
        static_cast<void>(breathing::tick(dolphin, kUnderwater));
    }
    CHECK(dolphin.airSupply == 4700);
    static_cast<void>(breathing::tick(dolphin, kDry));
    CHECK(dolphin.airSupply == 4800);
}
