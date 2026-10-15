#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::world::pressurePlates {
namespace {

/// parity: `TOUCH_AABB`.
constexpr double kEdge = 1.0 / 16.0;
constexpr double kHeight = 4.0 / 16.0;
constexpr std::int32_t kMaxSignal = 15;

[[nodiscard]] const wire::common::Identifier* plateAt(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || !block->path().ends_with("_pressure_plate")) {
        return nullptr;
    }
    return block;
}

[[nodiscard]] bool isWeighted(std::string_view path) {
    return path.starts_with("light_") || path.starts_with("heavy_");
}

[[nodiscard]] std::int32_t number(const std::map<std::string, std::string>& properties, const std::string& key,
                                  std::int32_t fallback) {
    const auto found = properties.find(key);
    if (found == properties.end()) {
        return fallback;
    }
    std::int32_t value = 0;
    const char* first = found->second.data();
    const char* last = first + found->second.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last ? value : fallback;
}

/// parity: `PressurePlateBlock.getSignalStrength` and `WeightedPressurePlateBlock`'s own.
[[nodiscard]] std::int32_t signalFor(std::string_view path, std::int32_t standing) {
    if (standing <= 0) {
        return 0;
    }
    std::int32_t maxWeight = 0;
    if (path == "light_weighted_pressure_plate") {
        maxWeight = kLightMaxWeight;
    } else if (path == "heavy_weighted_pressure_plate") {
        maxWeight = kHeavyMaxWeight;
    } else {
        return kMaxSignal;
    }
    const std::int32_t counted = std::min(standing, maxWeight);
    const float scaled = static_cast<float>(counted) / static_cast<float>(maxWeight) * kMaxSignal;
    return static_cast<std::int32_t>(std::ceil(scaled));
}

[[nodiscard]] std::map<std::string, std::string> stateProperties(std::string_view path, std::int32_t signal) {
    if (isWeighted(path)) {
        return {{"power", std::to_string(signal)}};
    }
    return {{"powered", signal > 0 ? "true" : "false"}};
}

[[nodiscard]] std::int32_t pressedTicks(std::string_view path) {
    return isWeighted(path) ? kWeightedPressedTicks : kPressedTicks;
}

} // namespace

bool isPlate(std::int32_t stateId) { return plateAt(stateId) != nullptr; }

std::optional<BlockChange> update(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Trigger from,
                                  std::int32_t standing, const std::int64_t gameTime) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = plateAt(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto powered = properties.find("powered");
    const std::int32_t showing =
        number(properties, "power", powered != properties.end() && powered->second == "true" ? kMaxSignal : 0);
    // parity: `entityInside` asks only while the plate reads zero and `tick` only while it reads.
    if ((from == Trigger::Touched) != (showing == 0)) {
        return std::nullopt;
    }
    const std::int32_t wanted = signalFor(path, standing);
    if (wanted > 0) {
        world.scheduleBlockTick(x, y, z, *block, pressedTicks(path));
    }
    if (wanted == showing) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, stateProperties(path, wanted), *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    // parity: `checkPressed` raises the event only when the plate goes from off to on or back.
    if ((wanted > 0) != (showing > 0) && world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, wanted > 0 ? "block_activate" : "block_deactivate", static_cast<double>(x) + 0.5,
                         static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, gameTime);
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

bool touches(const std::int32_t x, const std::int32_t y, const std::int32_t z, const double entityX,
             const double entityY, const double entityZ, const double halfWidth, const double height) {
    // parity: `AABB.intersects`, strict at both ends: touching is not pressing.
    const auto overlaps = [](double lowA, double highA, double lowB, double highB) {
        return lowA < highB && highA > lowB;
    };
    return overlaps(entityX - halfWidth, entityX + halfWidth, x + kEdge, x + 1 - kEdge) &&
           overlaps(entityY, entityY + height, y, y + kHeight) &&
           overlaps(entityZ - halfWidth, entityZ + halfWidth, z + kEdge, z + 1 - kEdge);
}

bool isStandingOn(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY, double entityZ) {
    return entityX >= x + kEdge && entityX <= x + 1 - kEdge && entityZ >= z + kEdge && entityZ <= z + 1 - kEdge &&
           entityY >= y && entityY <= y + kHeight;
}

std::int32_t blockUnder(double entityY) { return wire::common::mth::floor(entityY); }

} // namespace basalt::server::runtime::world::pressurePlates
