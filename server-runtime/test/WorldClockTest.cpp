#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/WorldClock.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
namespace rng = basalt::server::random;

TEST_CASE("a clock at the normal rate is one tick per tick") {
    WorldClock clock;
    CHECK(clock.totalTicks() == 0);
    CHECK(clock.rate() == doctest::Approx(1.0F));
    CHECK_FALSE(clock.paused());

    for (std::int32_t at = 1; at <= 100; ++at) {
        CHECK(clock.tick(true));
        CHECK(clock.totalTicks() == at);
    }
    CHECK(clock.partialTick() == doctest::Approx(0.0F));
}

TEST_CASE("half a rate is half a day, not every other tick") {
    WorldClock clock{0, 0.5F, false};
    // The first tick moves nothing: half a tick is not a tick, and the half is kept rather than
    // thrown away.
    CHECK_FALSE(clock.tick(true));
    CHECK(clock.totalTicks() == 0);
    CHECK(clock.partialTick() == doctest::Approx(0.5F));
    CHECK(clock.tick(true));
    CHECK(clock.totalTicks() == 1);
    CHECK(clock.partialTick() == doctest::Approx(0.0F));

    // A hundred ticks of half rate is fifty units of time, to the tick.
    for (std::int32_t at = 0; at < 98; ++at) {
        static_cast<void>(clock.tick(true));
    }
    CHECK(clock.totalTicks() == 50);
}

TEST_CASE("a rate above one moves more than a tick at a time") {
    WorldClock clock{0, 2.5F, false};
    CHECK(clock.tick(true));
    CHECK(clock.totalTicks() == 2);
    CHECK(clock.partialTick() == doctest::Approx(0.5F));
    CHECK(clock.tick(true));
    CHECK(clock.totalTicks() == 5);
}

TEST_CASE("a paused clock and a world that does not advance time") {
    WorldClock clock{500, 1.0F, true};
    CHECK_FALSE(clock.tick(true));
    CHECK(clock.totalTicks() == 500);
    // What the client is told, so it stops interpolating rather than drifting past the server.
    CHECK(clock.networkRate(true) == doctest::Approx(0.0F));

    clock.setPaused(false);
    CHECK(clock.networkRate(true) == doctest::Approx(1.0F));
    // The `advance_time` rule stops it just as a pause does.
    CHECK_FALSE(clock.tick(false));
    CHECK(clock.networkRate(false) == doctest::Approx(0.0F));
    CHECK(clock.totalTicks() == 500);
}

TEST_CASE("setting and adding time") {
    WorldClock clock{0, 0.5F, false};
    static_cast<void>(clock.tick(true));
    CHECK(clock.partialTick() == doctest::Approx(0.5F));

    // `/time set` starts the count again from a whole tick: the half that was owed is not carried
    // into the time the operator just asked for.
    clock.setTotalTicks(13000);
    CHECK(clock.totalTicks() == 13000);
    CHECK(clock.partialTick() == doctest::Approx(0.0F));

    clock.addTicks(1000);
    CHECK(clock.totalTicks() == 14000);
    // parity: `addTicks` clamps at zero rather than running the world backwards past its creation.
    clock.addTicks(-20000);
    CHECK(clock.totalTicks() == 0);
    clock.addTicks(-1);
    CHECK(clock.totalTicks() == 0);
}

TEST_CASE("the four markers a command names, and when each comes round again") {
    CHECK(WorldClock::timeMarkers().size() == 4);
    CHECK(WorldClock::markerTicks("day") == 1000);
    CHECK(WorldClock::markerTicks("noon") == 6000);
    CHECK(WorldClock::markerTicks("night") == 13000);
    CHECK(WorldClock::markerTicks("midnight") == 18000);
    CHECK_FALSE(WorldClock::markerTicks("dusk").has_value());

    // Later today.
    CHECK(WorldClock::timeOfNextMarker(0, 1000) == 1000);
    CHECK(WorldClock::timeOfNextMarker(500, 1000) == 1000);
    // Already past it, so tomorrow.
    CHECK(WorldClock::timeOfNextMarker(1000, 1000) == 25000);
    CHECK(WorldClock::timeOfNextMarker(2000, 1000) == 25000);
    // On a world that has been running for days, the answer is the next one, not the first one.
    CHECK(WorldClock::timeOfNextMarker(240500, 1000) == 241000);
    CHECK(WorldClock::timeOfNextMarker(241000, 18000) == 258000);

    // A clock wound back before its own epoch still asks for the next marker rather than one a
    // whole day later: the remainder has to be the floored one, not the truncated one.
    CHECK(WorldClock::timeOfNextMarker(-500, 1000) == 1000);
    CHECK(WorldClock::timeOfNextMarker(-23500, 1000) == -23000);
    CHECK(WorldClock::timeOfNextMarker(-25000, 1000) == -23000);
}

TEST_CASE("a fresh world draws a delay before it rains") {
    rng::RandomSource source = rng::RandomSource::legacy(42);
    WorldWeather weather;
    CHECK_FALSE(weather.raining);
    CHECK(weather.rainTime == 0);

    // Every counter at zero means the first tick draws a delay rather than flipping the weather on.
    weather.tick(source);
    CHECK_FALSE(weather.raining);
    CHECK_FALSE(weather.thundering);
    CHECK(weather.rainTime >= WorldWeather::kRainDelay.first);
    CHECK(weather.rainTime <= WorldWeather::kRainDelay.second);
    CHECK(weather.thunderTime >= WorldWeather::kThunderDelay.first);
    CHECK(weather.thunderTime <= WorldWeather::kThunderDelay.second);
}

TEST_CASE("a countdown that reaches zero flips the weather, and then draws a duration") {
    rng::RandomSource source = rng::RandomSource::legacy(7);
    WorldWeather weather;
    weather.rainTime = 2;
    weather.thunderTime = 1000000;

    weather.tick(source);
    CHECK(weather.rainTime == 1);
    CHECK_FALSE(weather.raining);
    weather.tick(source);
    CHECK(weather.rainTime == 0);
    CHECK(weather.raining);

    // Now that it is raining, the next draw is how long it lasts, not how long until it starts.
    weather.tick(source);
    CHECK(weather.raining);
    CHECK(weather.rainTime >= WorldWeather::kRainDuration.first);
    CHECK(weather.rainTime <= WorldWeather::kRainDuration.second);

    // And thunder is its own countdown, untouched by any of that.
    CHECK(weather.thunderTime == 1000000 - 3);
    CHECK_FALSE(weather.thundering);
}

TEST_CASE("thunder starts when its own countdown runs out") {
    rng::RandomSource source = rng::RandomSource::legacy(5);
    WorldWeather weather;
    weather.thunderTime = 2;
    weather.rainTime = 1000000;

    weather.tick(source);
    CHECK(weather.thunderTime == 1);
    CHECK_FALSE(weather.thundering);
    weather.tick(source);
    CHECK(weather.thunderTime == 0);
    CHECK(weather.thundering);
    // And the next tick draws how long the storm lasts.
    weather.tick(source);
    CHECK(weather.thunderTime >= WorldWeather::kThunderDuration.first);
}

TEST_CASE("what a command sets is both countdowns, not just the rain") {
    WorldWeather weather;
    weather.set(0, 500, true, true);
    CHECK(weather.clearWeatherTime == 0);
    CHECK(weather.rainTime == 500);
    // parity: `setWeatherParameters` gives thunder the same countdown, so `/weather thunder 25`
    // ends both at once instead of leaving a storm with no rain under it.
    CHECK(weather.thunderTime == 500);
    CHECK(weather.raining);
    CHECK(weather.thundering);

    weather.set(1200, 0, false, false);
    CHECK(weather.clearWeatherTime == 1200);
    CHECK(weather.rainTime == 0);
    CHECK(weather.thunderTime == 0);
    CHECK_FALSE(weather.raining);
}

TEST_CASE("thunder has its own duration once it starts") {
    rng::RandomSource source = rng::RandomSource::legacy(11);
    WorldWeather weather;
    weather.thundering = true;
    weather.raining = true;
    weather.thunderTime = 0;
    weather.rainTime = 1000000;

    weather.tick(source);
    CHECK(weather.thundering);
    CHECK(weather.thunderTime >= WorldWeather::kThunderDuration.first);
    CHECK(weather.thunderTime <= WorldWeather::kThunderDuration.second);
    // Which is a shorter range than the delay between storms.
    CHECK(weather.thunderTime <= WorldWeather::kThunderDelay.second);
}

TEST_CASE("a clear spell holds the sky open and leaves the cycle one tick from resuming") {
    rng::RandomSource source = rng::RandomSource::legacy(3);
    WorldWeather weather;
    weather.set(1000, 0, true, true);
    CHECK(weather.clearWeatherTime == 1000);
    CHECK(weather.raining);
    CHECK(weather.thundering);

    weather.tick(source);
    CHECK(weather.clearWeatherTime == 999);
    CHECK_FALSE(weather.raining);
    CHECK_FALSE(weather.thundering);
    // It was raining, so the counters say "flip now": that is what makes the rain come back the
    // moment the clear spell ends rather than after a fresh delay.
    CHECK(weather.rainTime == 0);
    CHECK(weather.thunderTime == 0);

    // While it runs, nothing else moves.
    weather.tick(source);
    CHECK(weather.clearWeatherTime == 998);
    CHECK(weather.rainTime == 1);
    CHECK(weather.thunderTime == 1);
    CHECK_FALSE(weather.raining);
}

TEST_CASE("what the client is shown is on or off, not a slope") {
    WorldWeather weather;
    CHECK(weather.rainLevel() == doctest::Approx(0.0F));
    CHECK(weather.thunderLevel() == doctest::Approx(0.0F));
    weather.raining = true;
    weather.thundering = true;
    CHECK(weather.rainLevel() == doctest::Approx(1.0F));
    CHECK(weather.thunderLevel() == doctest::Approx(1.0F));
}

TEST_CASE("a duration drawn for a command that gave none") {
    rng::RandomSource source = rng::RandomSource::legacy(99);
    for (std::int32_t at = 0; at < 200; ++at) {
        const std::int32_t drawn = WorldWeather::randomDuration(WorldWeather::kRainDuration, source);
        CAPTURE(at);
        CHECK(drawn >= 12000);
        CHECK(drawn <= 24000);
    }
    // Both ends are reachable, which a bound written one short would not be.
    rng::RandomSource narrow = rng::RandomSource::legacy(1);
    bool sawBottom = false;
    bool sawTop = false;
    for (std::int32_t at = 0; at < 2000; ++at) {
        const std::int32_t drawn = WorldWeather::randomDuration({1, 2}, narrow);
        sawBottom = sawBottom || drawn == 1;
        sawTop = sawTop || drawn == 2;
    }
    CHECK(sawBottom);
    CHECK(sawTop);
}
