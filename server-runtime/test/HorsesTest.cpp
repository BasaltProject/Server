#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

#include <memory>
#include <string>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace horses = basalt::server::runtime::entity::horses;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::unique_ptr<ServerEntity> spawn(std::int32_t id, const std::string& type) {
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(Identifier::vanilla(type));
    REQUIRE(entry != nullptr);
    auto mob = std::make_unique<ServerEntity>(id, Uuid{static_cast<std::uint64_t>(id), 1}, entry->id, entry->protocolId,
                                              0.0, 64.0, 0.0);
    mob->health = mob->attributeWithModifiers("minecraft:max_health");
    return mob;
}

void readyToBreed(ServerEntity& horse) {
    horse.tamedHorse = true;
    horse.loveTicks = 600;
}

} // namespace

TEST_CASE("a horse and a donkey make a mule, and only tame ones in love breed") {
    auto horse = spawn(1, "horse");
    auto donkey = spawn(2, "donkey");
    CHECK_FALSE(horses::canMate(*horse, *donkey));
    readyToBreed(*horse);
    readyToBreed(*donkey);
    CHECK(horses::canMate(*horse, *donkey));
    CHECK(horses::offspringType(*horse, *donkey).path() == "mule");
    auto mule = spawn(3, "mule");
    readyToBreed(*mule);
    CHECK_FALSE(horses::canMate(*mule, *horse));
    horse->passengerIds.push_back(9);
    CHECK_FALSE(horses::canMate(*horse, *donkey));
}

TEST_CASE("a foal's attributes stay between the kind's own bounds") {
    RandomSource random = RandomSource::legacy(7);
    for (std::int32_t roll = 0; roll < 200; ++roll) {
        const double value = horses::offspringAttribute(0.1125, 0.3375, 0.1125, 0.3375, random);
        CHECK(value >= 0.1125);
        CHECK(value <= 0.3375);
    }
}

TEST_CASE("a llama holds a third of a horse's temper and takes carpets, not saddles") {
    auto llama = spawn(1, "llama");
    CHECK(horses::maxTemper(llama->type()) == 30);
    CHECK(horses::modifyTemper(*llama, 50) == 30);
    CHECK(horses::wearableFor(*llama, Identifier::vanilla("red_carpet")) == horses::Wearable::Body);
    CHECK(horses::wearableFor(*llama, Identifier::vanilla("moss_carpet")) == horses::Wearable::None);
    CHECK(horses::wearableFor(*llama, Identifier::vanilla("saddle")) == horses::Wearable::None);
    auto donkey = spawn(2, "donkey");
    CHECK(horses::wearableFor(*donkey, Identifier::vanilla("saddle")) == horses::Wearable::None);
    donkey->tamedHorse = true;
    CHECK(horses::wearableFor(*donkey, Identifier::vanilla("saddle")) == horses::Wearable::Saddle);
    CHECK(horses::wearableFor(*donkey, Identifier::vanilla("iron_horse_armor")) == horses::Wearable::None);
}
