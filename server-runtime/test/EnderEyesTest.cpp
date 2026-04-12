#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>

#include <doctest.h>

#include <cmath>
#include <memory>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
namespace eyes = basalt::server::runtime::world::enderEyes;

namespace {

[[nodiscard]] std::unique_ptr<ServerEntity> eyeAt(double x, double y, double z) {
    return std::make_unique<ServerEntity>(1, basalt::wire::common::Uuid{1, 1}, eyes::type(), 0, x, y, z);
}

} // namespace

TEST_CASE("an eye aims twelve blocks and eight up towards a far feature, and straight at a near one") {
    RandomSource random = RandomSource::legacy(1);
    const std::unique_ptr<ServerEntity> far = eyeAt(0.0, 70.0, 0.0);
    eyes::signalTo(*far, 100.0, 0.0, 0.0, random);
    REQUIRE(far->enderEye != nullptr);
    CHECK((*far->enderEye->target)[0] == doctest::Approx(12.0));
    CHECK((*far->enderEye->target)[1] == doctest::Approx(78.0));

    const std::unique_ptr<ServerEntity> near = eyeAt(0.0, 70.0, 0.0);
    eyes::signalTo(*near, 5.0, 0.0, 5.0, random);
    CHECK((*near->enderEye->target)[1] == doctest::Approx(0.0));
}

TEST_CASE("an eye climbs towards its target and ends after eighty ticks") {
    RandomSource random = RandomSource::legacy(7);
    const std::unique_ptr<ServerEntity> eye = eyeAt(0.5, 70.0, 0.5);
    eyes::signalTo(*eye, 200.0, 0.0, 0.5, random);
    const bool survives = eye->enderEye->surviveAfterDeath;
    eyes::Fate fate = eyes::Fate::Flying;
    std::int32_t ticks = 0;
    while (fate == eyes::Fate::Flying) {
        fate = eyes::step(*eye);
        ++ticks;
    }
    CHECK(ticks == 81);
    CHECK(eye->removed);
    CHECK(fate == (survives ? eyes::Fate::Dropped : eyes::Fate::Shattered));
    CHECK(eye->x > 0.5);
    CHECK(eye->y > 70.0);
}
