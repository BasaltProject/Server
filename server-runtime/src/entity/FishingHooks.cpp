#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/FishingHooks.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::fishingHooks {
namespace {

namespace mth = wire::common::mth;

constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);
constexpr auto kPi = static_cast<float>(std::numbers::pi);
constexpr double kCastSpeed = 0.6;
constexpr double kCastSpread = 0.0103365;
constexpr float kMaxCastSlope = 5.0F;

} // namespace

std::int32_t nextIntBetween(random::RandomSource& random, std::int32_t low, std::int32_t high) {
    return low >= high ? low : random.nextInt(high - low + 1) + low;
}

float nextFloatBetween(random::RandomSource& random, float low, float high) {
    return low >= high ? low : (random.nextFloat() * (high - low)) + low;
}

void cast(ServerEntity& hook, HookState& state, double ownerX, double ownerEyeY, double ownerZ, float yaw,
          float pitch) {
    const float yCos = mth::cos((-yaw * kRadians) - kPi);
    const float ySin = mth::sin((-yaw * kRadians) - kPi);
    const float xCos = -mth::cos(-pitch * kRadians);
    const float xSin = mth::sin(-pitch * kRadians);
    hook.x = ownerX - (static_cast<double>(ySin) * 0.3);
    hook.y = ownerEyeY;
    hook.z = ownerZ - (static_cast<double>(yCos) * 0.3);
    double mx = static_cast<double>(-ySin);
    double my = static_cast<double>(std::clamp(-(xSin / xCos), -kMaxCastSlope, kMaxCastSlope));
    double mz = static_cast<double>(-yCos);
    const double dist = std::sqrt((mx * mx) + (my * my) + (mz * mz));
    // parity: three rolls, x then y then z, as Java evaluates the arguments.
    const double sx = (kCastSpeed / dist) + state.random.triangle(0.5, kCastSpread);
    const double sy = (kCastSpeed / dist) + state.random.triangle(0.5, kCastSpread);
    const double sz = (kCastSpeed / dist) + state.random.triangle(0.5, kCastSpread);
    mx *= sx;
    my *= sy;
    mz *= sz;
    hook.velocityX = mx;
    hook.velocityY = my;
    hook.velocityZ = mz;
    hook.yaw = static_cast<float>(enderDragon::vanillaAtan2(mx, mz) * 180.0 / static_cast<double>(kPi));
    hook.pitch = static_cast<float>(enderDragon::vanillaAtan2(my, std::sqrt((mx * mx) + (mz * mz))) * 180.0 /
                                    static_cast<double>(kPi));
}

void resetTimeUntilLured(HookState& state) {
    state.timeUntilLured = nextIntBetween(state.random, state.minWaitTime, state.maxWaitTime);
    state.timeUntilLured -=
        state.applyLure ? (state.lureSpeed >= state.maxWaitTime ? state.timeUntilLured - 1 : state.lureSpeed) : 0;
}

void catchingFish(ServerEntity& hook, HookState& state, const Surroundings& around) {
    random::RandomSource& random = state.random;
    std::int32_t fishingSpeed = 1;
    if (state.rainInfluenced && random.nextFloat() < 0.25F && around.rainingAbove) {
        ++fishingSpeed;
    }
    if (state.skyInfluenced && random.nextFloat() < 0.5F && !around.skyAbove) {
        --fishingSpeed;
    }
    const auto particles = [&around](std::string_view name, double x, double y, double z, std::int32_t count, double dx,
                                     double dy, double dz, double speed) {
        if (around.particles) {
            around.particles(name, x, y, z, count, dx, dy, dz, speed);
        }
    };
    const auto water = [&around](double x, double y, double z) {
        return around.isWater && around.isWater(mth::floor(x), mth::floor(y), mth::floor(z));
    };
    if (state.nibble > 0) {
        --state.nibble;
        if (state.nibble <= 0) {
            state.timeUntilLured = 0;
            state.timeUntilHooked = 0;
            state.biting = false;
        }
    } else if (state.timeUntilHooked > 0) {
        state.timeUntilHooked -= fishingSpeed;
        if (state.timeUntilHooked > 0) {
            state.fishAngle += static_cast<float>(random.triangle(0.0, 9.188));
            const float angle = state.fishAngle * kRadians;
            const float angleSin = mth::sin(angle);
            const float angleCos = mth::cos(angle);
            const double fishX =
                hook.x + static_cast<double>(angleSin * static_cast<float>(state.timeUntilHooked) * 0.1F);
            const double fishY = static_cast<double>(static_cast<float>(mth::floor(hook.y)) + 1.0F);
            const double fishZ =
                hook.z + static_cast<double>(angleCos * static_cast<float>(state.timeUntilHooked) * 0.1F);
            if (water(fishX, fishY - 1.0, fishZ)) {
                if (random.nextFloat() < 0.15F) {
                    particles("bubble", fishX, fishY - static_cast<double>(0.1F), fishZ, 1,
                              static_cast<double>(angleSin), 0.1, static_cast<double>(angleCos), 0.0);
                }
                const float moveX = angleSin * 0.04F;
                const float moveZ = angleCos * 0.04F;
                particles("fishing", fishX, fishY, fishZ, 0, static_cast<double>(moveZ), 0.01,
                          static_cast<double>(-moveX), 1.0);
                particles("fishing", fishX, fishY, fishZ, 0, static_cast<double>(-moveZ), 0.01,
                          static_cast<double>(moveX), 1.0);
            }
        } else {
            const float pitch = 1.0F + ((random.nextFloat() - random.nextFloat()) * 0.4F);
            if (around.splash) {
                around.splash(0.25F, pitch);
            }
            const double y = hook.y + 0.5;
            const double width = hook.width();
            const auto count = static_cast<std::int32_t>(1.0F + (static_cast<float>(width) * 20.0F));
            particles("bubble", hook.x, y, hook.z, count, width, 0.0, width, static_cast<double>(0.2F));
            particles("fishing", hook.x, y, hook.z, count, width, 0.0, width, static_cast<double>(0.2F));
            state.nibble = nextIntBetween(random, 20, 40);
            state.biting = true;
        }
    } else if (state.timeUntilLured > 0) {
        state.timeUntilLured -= fishingSpeed;
        float teaseChance = 0.15F;
        if (state.timeUntilLured < 20) {
            teaseChance += static_cast<float>(20 - state.timeUntilLured) * 0.05F;
        } else if (state.timeUntilLured < 40) {
            teaseChance += static_cast<float>(40 - state.timeUntilLured) * 0.02F;
        } else if (state.timeUntilLured < 60) {
            teaseChance += static_cast<float>(60 - state.timeUntilLured) * 0.01F;
        }
        if (random.nextFloat() < teaseChance) {
            const float angle = nextFloatBetween(random, 0.0F, 360.0F) * kRadians;
            const float dist = nextFloatBetween(random, 25.0F, 60.0F);
            const double fishX = hook.x + (static_cast<double>(mth::sin(angle) * dist) * 0.1);
            const double fishY = static_cast<double>(static_cast<float>(mth::floor(hook.y)) + 1.0F);
            const double fishZ = hook.z + (static_cast<double>(mth::cos(angle) * dist) * 0.1);
            if (water(fishX, fishY - 1.0, fishZ)) {
                const std::int32_t count = 2 + random.nextInt(2);
                particles("splash", fishX, fishY, fishZ, count, static_cast<double>(0.1F), 0.0,
                          static_cast<double>(0.1F), 0.0);
            }
        }
        if (state.timeUntilLured <= 0) {
            state.fishAngle = nextFloatBetween(random, state.minLureAngle, state.maxLureAngle);
            state.timeUntilHooked = nextIntBetween(random, state.minLureTime, state.maxLureTime);
        }
    } else {
        resetTimeUntilLured(state);
    }
}

bool calculateOpenWater(std::int32_t x, std::int32_t y, std::int32_t z,
                        const std::function<WaterLayer(std::int32_t, std::int32_t, std::int32_t)>& layerAt) {
    WaterLayer previous = WaterLayer::Invalid;
    for (std::int32_t dy = -1; dy <= 2; ++dy) {
        // parity: `getOpenWaterTypeForArea`, every block the same kind or the layer is invalid.
        std::optional<WaterLayer> layer;
        for (std::int32_t dx = -2; dx <= 2; ++dx) {
            for (std::int32_t dz = -2; dz <= 2; ++dz) {
                const WaterLayer here = layerAt(x + dx, y + dy, z + dz);
                layer = !layer.has_value() || *layer == here ? here : WaterLayer::Invalid;
            }
        }
        switch (layer.value_or(WaterLayer::Invalid)) {
        case WaterLayer::AboveWater:
            if (previous == WaterLayer::Invalid) {
                return false;
            }
            break;
        case WaterLayer::InsideWater:
            if (previous == WaterLayer::AboveWater) {
                return false;
            }
            break;
        case WaterLayer::Invalid:
            return false;
        }
        previous = *layer;
    }
    return true;
}

} // namespace basalt::server::runtime::entity::fishingHooks
