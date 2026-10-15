#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/noise/JavaMath.hpp>
#include <basalt/server/world/terrain/NoiseChunk.hpp>
#include <basalt/server/world/terrain/carver/Carvers.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <variant>

namespace basalt::server::world::terrain::carver {
namespace {

namespace mth = wire::common::mth;
namespace jm = noise::javaMath;
using wire::common::JsonObject;
using wire::common::JsonValue;

constexpr std::int32_t kSourceRange = 8;
constexpr std::int32_t kProtectedBlocksOnTop = 7;
/// parity: `SectionPos.sectionToBlockCoord(getRange() * 2 - 1)`, both carver kinds.
constexpr std::int32_t kMaxDistance = 112;
constexpr auto kPiFloat = static_cast<float>(std::numbers::pi);
constexpr auto kHalfPi = static_cast<float>(std::numbers::pi / 2.0);
constexpr auto kTwoPi = static_cast<float>(std::numbers::pi * 2.0);

[[nodiscard]] std::optional<double> number(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    if (const auto* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    return std::nullopt;
}

[[nodiscard]] std::string typeOf(const JsonObject& object) {
    const JsonValue* value = object.find("type");
    const auto* text = value == nullptr ? nullptr : value->get<std::string>();
    std::string type = text == nullptr ? std::string{} : *text;
    return type.starts_with("minecraft:") ? type.substr(10) : type;
}

/// parity: `FloatProvider`, the kinds the carvers use.
struct FloatProvider {
    enum class Kind : std::uint8_t { Constant, Uniform, Trapezoid };
    Kind kind = Kind::Constant;
    float a = 0.0F;
    float b = 0.0F;
    float c = 0.0F;

    [[nodiscard]] float sample(random::RandomSource& random) const {
        switch (kind) {
        case Kind::Constant:
            return a;
        case Kind::Uniform:
            return random.nextFloat() * (b - a) + a;
        case Kind::Trapezoid: {
            const float range = b - a;
            const float plateauStart = (range - c) / 2.0F;
            const float plateauEnd = range - plateauStart;
            return a + random.nextFloat() * plateauEnd + random.nextFloat() * plateauStart;
        }
        }
        return a;
    }
};

[[nodiscard]] FloatProvider floatProvider(const JsonValue* json) {
    if (const std::optional<double> constant = number(json); constant.has_value()) {
        return FloatProvider{FloatProvider::Kind::Constant, static_cast<float>(*constant)};
    }
    const auto* object = json == nullptr ? nullptr : json->get<JsonObject>();
    if (object == nullptr) {
        throw std::runtime_error{"carver: invalid float provider"};
    }
    const std::string type = typeOf(*object);
    if (type == "constant") {
        return FloatProvider{FloatProvider::Kind::Constant,
                             static_cast<float>(number(object->find("value")).value_or(0.0))};
    }
    if (type == "uniform") {
        return FloatProvider{FloatProvider::Kind::Uniform,
                             static_cast<float>(number(object->find("min_inclusive")).value_or(0.0)),
                             static_cast<float>(number(object->find("max_exclusive")).value_or(0.0))};
    }
    if (type == "trapezoid") {
        return FloatProvider{FloatProvider::Kind::Trapezoid,
                             static_cast<float>(number(object->find("min")).value_or(0.0)),
                             static_cast<float>(number(object->find("max")).value_or(0.0)),
                             static_cast<float>(number(object->find("plateau")).value_or(0.0))};
    }
    throw std::runtime_error{"carver: unsupported float provider '" + type + "'"};
}

/// parity: `IntProvider`, the kinds the carvers use.
struct IntProvider {
    enum class Kind : std::uint8_t { Constant, Uniform, BiasedToBottom, VeryBiasedToBottom };
    Kind kind = Kind::Constant;
    std::int32_t min = 0;
    std::int32_t max = 0;

    [[nodiscard]] std::int32_t sample(random::RandomSource& random) const {
        switch (kind) {
        case Kind::Constant:
            return min;
        case Kind::Uniform:
            return random.nextInt(max - min + 1) + min;
        case Kind::BiasedToBottom:
            return min + random.nextInt(random.nextInt(max - min + 1) + 1);
        case Kind::VeryBiasedToBottom:
            return min + random.nextInt(random.nextInt(random.nextInt(max - min + 1) + 1) + 1);
        }
        return min;
    }
};

[[nodiscard]] IntProvider intProvider(const JsonValue* json) {
    if (const std::optional<double> constant = number(json); constant.has_value()) {
        return IntProvider{IntProvider::Kind::Constant, static_cast<std::int32_t>(*constant)};
    }
    const auto* object = json == nullptr ? nullptr : json->get<JsonObject>();
    if (object == nullptr) {
        throw std::runtime_error{"carver: invalid int provider"};
    }
    const std::string type = typeOf(*object);
    const auto min = static_cast<std::int32_t>(number(object->find("min_inclusive")).value_or(0.0));
    const auto max = static_cast<std::int32_t>(number(object->find("max_inclusive")).value_or(0.0));
    if (type == "constant") {
        return IntProvider{IntProvider::Kind::Constant,
                           static_cast<std::int32_t>(number(object->find("value")).value_or(0.0))};
    }
    if (type == "uniform") {
        return IntProvider{IntProvider::Kind::Uniform, min, max};
    }
    if (type == "biased_to_bottom") {
        return IntProvider{IntProvider::Kind::BiasedToBottom, min, max};
    }
    if (type == "very_biased_to_bottom") {
        return IntProvider{IntProvider::Kind::VeryBiasedToBottom, min, max};
    }
    throw std::runtime_error{"carver: unsupported int provider '" + type + "'"};
}

/// parity: `VerticalAnchor`.
struct Anchor {
    std::int32_t kind = 0;
    std::int32_t value = 0;

    [[nodiscard]] std::int32_t resolve(std::int32_t minGenY, std::int32_t genDepth) const noexcept {
        return kind == 0 ? value : (kind == 1 ? minGenY + value : minGenY + genDepth - 1 - value);
    }
};

[[nodiscard]] Anchor anchor(const JsonValue* json) {
    const auto* object = json == nullptr ? nullptr : json->get<JsonObject>();
    if (object != nullptr) {
        if (const std::optional<double> value = number(object->find("absolute")); value.has_value()) {
            return Anchor{0, static_cast<std::int32_t>(*value)};
        }
        if (const std::optional<double> value = number(object->find("above_bottom")); value.has_value()) {
            return Anchor{1, static_cast<std::int32_t>(*value)};
        }
        if (const std::optional<double> value = number(object->find("below_top")); value.has_value()) {
            return Anchor{2, static_cast<std::int32_t>(*value)};
        }
    }
    throw std::runtime_error{"carver: invalid vertical anchor"};
}

/// parity: `UniformHeight`, the one height provider the carvers use.
struct HeightProvider {
    Anchor min;
    Anchor max;

    [[nodiscard]] std::int32_t sample(random::RandomSource& random, std::int32_t minGenY, std::int32_t genDepth) const {
        const std::int32_t low = min.resolve(minGenY, genDepth);
        const std::int32_t high = max.resolve(minGenY, genDepth);
        return low > high ? low : random.nextInt(high - low + 1) + low;
    }
};

[[nodiscard]] HeightProvider heightProvider(const JsonValue* json) {
    const auto* object = json == nullptr ? nullptr : json->get<JsonObject>();
    if (object == nullptr || typeOf(*object) != "uniform") {
        throw std::runtime_error{"carver: unsupported height provider"};
    }
    return HeightProvider{anchor(object->find("min_inclusive")), anchor(object->find("max_inclusive"))};
}

using SkipChecker = std::function<bool(double, double, double, std::int32_t)>;

/// parity: `WorldCarver.canReach`.
[[nodiscard]] bool canReach(std::int32_t chunkX, std::int32_t chunkZ, double x, double z, std::int32_t currentStep,
                            std::int32_t totalSteps, float thickness) {
    const double xd = x - static_cast<double>(chunkX * 16 + 8);
    const double zd = z - static_cast<double>(chunkZ * 16 + 8);
    const auto remaining = static_cast<double>(totalSteps - currentStep);
    const auto radius = static_cast<double>(thickness + 2.0F + 16.0F);
    return xd * xd + zd * zd - remaining * remaining <= radius * radius;
}

/// parity: `WorldCarver.carveEllipsoid`, which since 26.3 only marks the mask.
void carveEllipsoid(std::int32_t chunkX, std::int32_t chunkZ, double x, double y, double z, double horizontalRadius,
                    double verticalRadius, CarvingMask& mask, const SkipChecker& skip) {
    const auto centerX = static_cast<double>(chunkX * 16 + 8);
    const auto centerZ = static_cast<double>(chunkZ * 16 + 8);
    const double maxDelta = 16.0 + horizontalRadius * 2.0;
    if (std::fabs(x - centerX) > maxDelta || std::fabs(z - centerZ) > maxDelta) {
        return;
    }
    const std::int32_t chunkMinX = chunkX * 16;
    const std::int32_t chunkMinZ = chunkZ * 16;
    const std::int32_t minXIndex = std::max(jm::floor(x - horizontalRadius) - chunkMinX - 1, 0);
    const std::int32_t maxXIndex = std::min(jm::floor(x + horizontalRadius) - chunkMinX, 15);
    const std::int32_t minY = std::max(jm::floor(y - verticalRadius) - 1, mask.minY);
    const std::int32_t maxY = std::min(jm::floor(y + verticalRadius) + 1, mask.maxY);
    const std::int32_t minZIndex = std::max(jm::floor(z - horizontalRadius) - chunkMinZ - 1, 0);
    const std::int32_t maxZIndex = std::min(jm::floor(z + horizontalRadius) - chunkMinZ, 15);
    for (std::int32_t xIndex = minXIndex; xIndex <= maxXIndex; ++xIndex) {
        const double xd = (static_cast<double>(chunkMinX + xIndex) + 0.5 - x) / horizontalRadius;
        for (std::int32_t zIndex = minZIndex; zIndex <= maxZIndex; ++zIndex) {
            const double zd = (static_cast<double>(chunkMinZ + zIndex) + 0.5 - z) / horizontalRadius;
            if (xd * xd + zd * zd >= 1.0) {
                continue;
            }
            for (std::int32_t worldY = maxY; worldY > minY; --worldY) {
                const double yd = (static_cast<double>(worldY) - 0.5 - y) / verticalRadius;
                if (!skip(xd, yd, zd, worldY)) {
                    mask.set(xIndex, worldY, zIndex);
                }
            }
        }
    }
}

/// parity: `CaveWorldCarver`.
class CaveCarver final : public WorldCarver {
public:
    explicit CaveCarver(const JsonObject& json)
        : probability_(static_cast<float>(number(json.find("probability")).value_or(0.0))),
          y_(heightProvider(json.find("y"))), count_(intProvider(json.find("count"))),
          thickness_(floatProvider(json.find("thickness"))),
          roomVertical_(floatProvider(json.find("room_vertical_radius_multiplier"))),
          horizontal_(floatProvider(json.find("horizontal_radius_multiplier"))),
          vertical_(floatProvider(json.find("vertical_radius_multiplier"))),
          startVertical_(json.find("start_vertical_radius_multiplier") == nullptr
                             ? FloatProvider{FloatProvider::Kind::Constant, 1.0F}
                             : floatProvider(json.find("start_vertical_radius_multiplier"))),
          floorLevel_(floatProvider(json.find("floor_level"))) {
        const JsonValue* bias = json.find("weird_thickness_bias");
        weirdThicknessBias_ = bias != nullptr && bias->get<bool>() != nullptr && *bias->get<bool>();
    }

    [[nodiscard]] bool isStartChunk(random::RandomSource& random) const override {
        return random.nextFloat() <= probability_;
    }

    void carve(random::RandomSource& random, std::int32_t chunkX, std::int32_t chunkZ, std::int32_t sourceX,
               std::int32_t sourceZ, std::int32_t minGenY, std::int32_t genDepth, CarvingMask& mask) const override {
        const std::int32_t caveCount = count_.sample(random);
        for (std::int32_t cave = 0; cave < caveCount; ++cave) {
            const auto x = static_cast<double>(sourceX * 16 + random.nextInt(16));
            const auto y = static_cast<double>(y_.sample(random, minGenY, genDepth));
            const auto z = static_cast<double>(sourceZ * 16 + random.nextInt(16));
            const auto horizontalMultiplier = static_cast<double>(horizontal_.sample(random));
            const auto verticalMultiplier = static_cast<double>(vertical_.sample(random));
            const auto startVerticalMultiplier = static_cast<double>(startVertical_.sample(random));
            const auto floorLevel = static_cast<double>(floorLevel_.sample(random));
            const SkipChecker skip = [floorLevel](double xd, double yd, double zd, std::int32_t) {
                return yd <= floorLevel || xd * xd + yd * yd + zd * zd >= 1.0;
            };
            std::int32_t tunnels = 1;
            if (random.nextInt(4) == 0) {
                const auto yScale = static_cast<double>(roomVertical_.sample(random));
                const float thickness = 1.0F + random.nextFloat() * 6.0F;
                const double horizontalRadius = 1.5 + static_cast<double>(mth::sin(kHalfPi) * thickness);
                carveEllipsoid(chunkX, chunkZ, x + 1.0, y, z, horizontalRadius, horizontalRadius * yScale, mask, skip);
                tunnels += random.nextInt(4);
            }
            for (std::int32_t i = 0; i < tunnels; ++i) {
                const float horizontalRotation = random.nextFloat() * kTwoPi;
                const float verticalRotation = (random.nextFloat() - 0.5F) / 4.0F;
                float thickness = thickness_.sample(random);
                if (weirdThicknessBias_ && random.nextInt(10) == 0) {
                    thickness *= random.nextFloat() * random.nextFloat() * 3.0F + 1.0F;
                }
                const std::int32_t distance = kMaxDistance - random.nextInt(kMaxDistance / 4);
                createTunnel(chunkX, chunkZ, random.nextLong(), x, y, z, horizontalMultiplier, verticalMultiplier,
                             thickness, horizontalRotation, verticalRotation, 0, distance, startVerticalMultiplier,
                             mask, skip);
            }
        }
    }

private:
    void createTunnel(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed, double x, double y, double z,
                      double horizontalMultiplier, double verticalMultiplier, float thickness, float horizontalRotation,
                      float verticalRotation, std::int32_t step, std::int32_t distance, double yScale,
                      CarvingMask& mask, const SkipChecker& skip) const {
        random::RandomSource random = random::RandomSource::legacy(seed);
        const std::int32_t splitPoint = random.nextInt(distance / 2) + distance / 4;
        const bool steep = random.nextInt(6) == 0;
        float yRota = 0.0F;
        float xRota = 0.0F;
        for (std::int32_t currentStep = step; currentStep < distance; ++currentStep) {
            const double horizontalRadius =
                1.5 +
                static_cast<double>(
                    mth::sin(kPiFloat * static_cast<float>(currentStep) / static_cast<float>(distance)) * thickness);
            const double verticalRadius = horizontalRadius * yScale;
            const float cosX = mth::cos(verticalRotation);
            x += static_cast<double>(mth::cos(horizontalRotation) * cosX);
            y += static_cast<double>(mth::sin(verticalRotation));
            z += static_cast<double>(mth::sin(horizontalRotation) * cosX);
            verticalRotation *= steep ? 0.92F : 0.7F;
            verticalRotation += xRota * 0.1F;
            horizontalRotation += yRota * 0.1F;
            xRota *= 0.9F;
            yRota *= 0.75F;
            xRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0F;
            yRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0F;
            if (currentStep == splitPoint && thickness > 1.0F) {
                const std::int64_t first = random.nextLong();
                const float firstThickness = random.nextFloat() * 0.5F + 0.5F;
                createTunnel(chunkX, chunkZ, first, x, y, z, horizontalMultiplier, verticalMultiplier, firstThickness,
                             horizontalRotation - kHalfPi, verticalRotation / 3.0F, currentStep, distance, 1.0, mask,
                             skip);
                const std::int64_t second = random.nextLong();
                const float secondThickness = random.nextFloat() * 0.5F + 0.5F;
                createTunnel(chunkX, chunkZ, second, x, y, z, horizontalMultiplier, verticalMultiplier, secondThickness,
                             horizontalRotation + kHalfPi, verticalRotation / 3.0F, currentStep, distance, 1.0, mask,
                             skip);
                return;
            }
            if (random.nextInt(4) != 0) {
                if (!canReach(chunkX, chunkZ, x, z, currentStep, distance, thickness)) {
                    return;
                }
                carveEllipsoid(chunkX, chunkZ, x, y, z, horizontalRadius * horizontalMultiplier,
                               verticalRadius * verticalMultiplier, mask, skip);
            }
        }
    }

    float probability_;
    HeightProvider y_;
    IntProvider count_;
    FloatProvider thickness_;
    FloatProvider roomVertical_;
    FloatProvider horizontal_;
    FloatProvider vertical_;
    FloatProvider startVertical_;
    FloatProvider floorLevel_;
    bool weirdThicknessBias_ = false;
};

/// parity: `CanyonWorldCarver`.
class CanyonCarver final : public WorldCarver {
public:
    explicit CanyonCarver(const JsonObject& json)
        : probability_(static_cast<float>(number(json.find("probability")).value_or(0.0))),
          y_(heightProvider(json.find("y"))), verticalRotation_(floatProvider(json.find("vertical_rotation"))) {
        const JsonValue* shapeField = json.find("shape");
        const auto* shape = shapeField == nullptr ? nullptr : shapeField->get<JsonObject>();
        if (shape == nullptr) {
            throw std::runtime_error{"canyon: missing 'shape'"};
        }
        distanceFactor_ = floatProvider(shape->find("distance_factor"));
        thickness_ = floatProvider(shape->find("thickness"));
        widthSmoothness_ = static_cast<std::int32_t>(number(shape->find("width_smoothness")).value_or(1.0));
        horizontalRadiusFactor_ = floatProvider(shape->find("horizontal_radius_factor"));
        verticalDefault_ = static_cast<float>(number(shape->find("vertical_radius_default_factor")).value_or(0.0));
        verticalCenter_ = static_cast<float>(number(shape->find("vertical_radius_center_factor")).value_or(0.0));
        yScale_ = floatProvider(shape->find("y_scale"));
    }

    [[nodiscard]] bool isStartChunk(random::RandomSource& random) const override {
        return random.nextFloat() <= probability_;
    }

    void carve(random::RandomSource& random, std::int32_t chunkX, std::int32_t chunkZ, std::int32_t sourceX,
               std::int32_t sourceZ, std::int32_t minGenY, std::int32_t genDepth, CarvingMask& mask) const override {
        const auto x = static_cast<double>(sourceX * 16 + random.nextInt(16));
        const std::int32_t y = y_.sample(random, minGenY, genDepth);
        const auto z = static_cast<double>(sourceZ * 16 + random.nextInt(16));
        const float horizontalRotation = random.nextFloat() * kTwoPi;
        const float verticalRotation = verticalRotation_.sample(random);
        const auto yScale = static_cast<double>(yScale_.sample(random));
        const float thickness = thickness_.sample(random);
        const auto distance =
            jm::toInt(static_cast<double>(static_cast<float>(kMaxDistance) * distanceFactor_.sample(random)));
        doCarve(chunkX, chunkZ, random.nextLong(), x, static_cast<double>(y), z, thickness, horizontalRotation,
                verticalRotation, distance, yScale, minGenY, genDepth, mask);
    }

private:
    void doCarve(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed, double x, double y, double z,
                 float thickness, float horizontalRotation, float verticalRotation, std::int32_t distance,
                 double yScale, std::int32_t minGenY, std::int32_t genDepth, CarvingMask& mask) const {
        random::RandomSource random = random::RandomSource::legacy(seed);
        std::vector<float> widths(static_cast<std::size_t>(genDepth));
        float width = 1.0F;
        for (std::int32_t index = 0; index < genDepth; ++index) {
            if (index == 0 || random.nextInt(widthSmoothness_) == 0) {
                width = 1.0F + random.nextFloat() * random.nextFloat();
            }
            widths[static_cast<std::size_t>(index)] = width * width;
        }
        const SkipChecker skip = [&widths, minGenY](double xd, double yd, double zd, std::int32_t worldY) {
            const std::int32_t index = worldY - minGenY;
            return (xd * xd + zd * zd) * static_cast<double>(widths[static_cast<std::size_t>(index - 1)]) +
                       yd * yd / 6.0 >=
                   1.0;
        };
        float yRota = 0.0F;
        float xRota = 0.0F;
        for (std::int32_t currentStep = 0; currentStep < distance; ++currentStep) {
            double horizontalRadius = 1.5 + static_cast<double>(mth::sin(static_cast<float>(currentStep) * kPiFloat /
                                                                         static_cast<float>(distance)) *
                                                                thickness);
            double verticalRadius = horizontalRadius * yScale;
            horizontalRadius *= static_cast<double>(horizontalRadiusFactor_.sample(random));
            const float verticalMultiplier =
                1.0F - std::fabs(0.5F - static_cast<float>(currentStep) / static_cast<float>(distance)) * 2.0F;
            const float factor = verticalDefault_ + verticalCenter_ * verticalMultiplier;
            verticalRadius = static_cast<double>(factor) * verticalRadius *
                             static_cast<double>(random.nextFloat() * (1.0F - 0.75F) + 0.75F);
            const float xc = mth::cos(verticalRotation);
            const float xs = mth::sin(verticalRotation);
            x += static_cast<double>(mth::cos(horizontalRotation) * xc);
            y += static_cast<double>(xs);
            z += static_cast<double>(mth::sin(horizontalRotation) * xc);
            verticalRotation *= 0.7F;
            verticalRotation += xRota * 0.05F;
            horizontalRotation += yRota * 0.05F;
            xRota *= 0.8F;
            yRota *= 0.5F;
            xRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0F;
            yRota += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0F;
            if (random.nextInt(4) != 0) {
                if (!canReach(chunkX, chunkZ, x, z, currentStep, distance, thickness)) {
                    return;
                }
                carveEllipsoid(chunkX, chunkZ, x, y, z, horizontalRadius, verticalRadius, mask, skip);
            }
        }
    }

    float probability_;
    HeightProvider y_;
    FloatProvider verticalRotation_;
    FloatProvider distanceFactor_;
    FloatProvider thickness_;
    std::int32_t widthSmoothness_ = 1;
    FloatProvider horizontalRadiusFactor_;
    float verticalDefault_ = 1.0F;
    float verticalCenter_ = 0.0F;
    FloatProvider yScale_;
};

[[nodiscard]] std::string withNamespace(std::string_view id) {
    return id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
}

} // namespace

CarverRegistry::CarverRegistry(densityfunction::WorldgenReader reader) : reader_(std::move(reader)) {}

std::shared_ptr<const WorldCarver> CarverRegistry::carver(const std::string& id) {
    if (const auto found = carvers_.find(id); found != carvers_.end()) {
        return found->second;
    }
    const std::optional<std::string> text = reader_(densityfunction::resourcePath("worldgen/carver", id));
    const std::expected<JsonValue, wire::common::JsonError> json =
        text.has_value() ? wire::common::parseJson(*text) : std::unexpected(wire::common::JsonError::SyntaxError);
    const auto* object = json.has_value() ? json->get<JsonObject>() : nullptr;
    if (object == nullptr) {
        throw std::runtime_error{"carver '" + id + "' is missing or malformed"};
    }
    const std::string type = typeOf(*object);
    std::shared_ptr<const WorldCarver> made;
    if (type == "cave") {
        made = std::make_shared<const CaveCarver>(*object);
    } else if (type == "canyon") {
        made = std::make_shared<const CanyonCarver>(*object);
    } else {
        throw std::runtime_error{"carver '" + id + "' has unsupported type '" + type + "'"};
    }
    carvers_.emplace(id, made);
    return made;
}

const std::vector<std::shared_ptr<const WorldCarver>>& CarverRegistry::forBiome(const std::string& biome) {
    const std::lock_guard<std::mutex> guard{mutex_};
    const std::string key = withNamespace(biome);
    if (const auto found = biomes_.find(key); found != biomes_.end()) {
        return found->second;
    }
    const std::optional<std::string> text = reader_(densityfunction::resourcePath("worldgen/biome", key));
    const std::expected<JsonValue, wire::common::JsonError> json =
        text.has_value() ? wire::common::parseJson(*text) : std::unexpected(wire::common::JsonError::SyntaxError);
    const auto* object = json.has_value() ? json->get<JsonObject>() : nullptr;
    if (object == nullptr) {
        throw std::runtime_error{"biome '" + key + "' is missing or malformed"};
    }
    std::vector<std::shared_ptr<const WorldCarver>> list;
    if (const JsonValue* carvers = object->find("carvers"); carvers != nullptr) {
        if (const auto* single = carvers->get<std::string>(); single != nullptr) {
            list.push_back(carver(withNamespace(*single)));
        } else if (const auto* many = carvers->get<std::vector<JsonValue>>(); many != nullptr) {
            for (const JsonValue& entry : *many) {
                if (const auto* id = entry.get<std::string>(); id != nullptr) {
                    list.push_back(carver(withNamespace(*id)));
                }
            }
        }
    }
    return biomes_.emplace(key, std::move(list)).first->second;
}

void carve(Chunk& chunk, std::int64_t seed, NoiseChunk& noiseChunk, const NoiseRouter& router, CarverRegistry& registry,
           const CarverBiome& biomeAt, const TopMaterial& topMaterial) {
    const std::int32_t minGenY = router.minY;
    const std::int32_t genDepth = router.height;
    CarvingMask mask{minGenY + 1, minGenY + genDepth - 1 - kProtectedBlocksOnTop};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0LL));
    for (std::int32_t dx = -kSourceRange; dx <= kSourceRange; ++dx) {
        for (std::int32_t dz = -kSourceRange; dz <= kSourceRange; ++dz) {
            const std::int32_t sourceX = chunk.chunkX() + dx;
            const std::int32_t sourceZ = chunk.chunkZ() + dz;
            const std::string& biome = biomeAt(sourceX << 2, 0, sourceZ << 2);
            const std::vector<std::shared_ptr<const WorldCarver>>& carvers = registry.forBiome(biome);
            for (std::size_t index = 0; index < carvers.size(); ++index) {
                random::setLargeFeatureSeed(random, seed + static_cast<std::int64_t>(index), sourceX, sourceZ);
                if (carvers[index]->isStartChunk(random)) {
                    carvers[index]->carve(random, chunk.chunkX(), chunk.chunkZ(), sourceX, sourceZ, minGenY, genDepth,
                                          mask);
                }
            }
        }
    }
    // parity: `applyCarvingMask`.
    const std::int32_t height = mask.height();
    const std::int32_t bedrock = registry::blocks::bedrock().stateId;
    const std::int32_t dirt = registry::blocks::named("dirt")->stateId;
    Aquifer& aquifer = noiseChunk.aquifer();
    const auto total = static_cast<std::int32_t>(mask.bits.size());
    std::int32_t index = 0;
    while (index < total) {
        if (!mask.bits[static_cast<std::size_t>(index)]) {
            ++index;
            continue;
        }
        std::int32_t end = index;
        while (end + 1 < total && mask.bits[static_cast<std::size_t>(end + 1)]) {
            ++end;
        }
        for (std::int32_t column = index / height; column <= end / height; ++column) {
            const std::int32_t localX = (column >> 4) & 15;
            const std::int32_t localZ = column & 15;
            const std::int32_t base = column * height;
            const std::int32_t bottomY = std::max(index - base, 0) + mask.minY;
            const std::int32_t topY = std::min(end - base, height - 1) + mask.minY;
            const std::int32_t worldX = chunk.chunkX() * 16 + localX;
            const std::int32_t worldZ = chunk.chunkZ() * 16 + localZ;
            bool hasGrass = false;
            for (std::int32_t worldY = topY; worldY >= bottomY; --worldY) {
                const std::int32_t old = chunk.blockStateId(localX, worldY, localZ);
                if (old == bedrock) {
                    continue;
                }
                const std::optional<registry::BlockState> oldState = registry::blocks::byStateId(old);
                if (oldState.has_value() &&
                    (oldState->block.path() == "grass_block" || oldState->block.path() == "mycelium")) {
                    hasGrass = true;
                }
                const std::optional<std::int32_t> state = aquifer.computeSubstance(worldX, worldY, worldZ, 0.0);
                if (!state.has_value()) {
                    continue;
                }
                chunk.setBlockStateId(localX, worldY, localZ, *state);
                const bool fluid = registry::blockFluids::hasFluid(*state);
                if (aquifer.shouldScheduleFluidUpdate() && fluid) {
                    chunk.markFluidUpdate(worldX, worldY, worldZ);
                }
                if (hasGrass && worldY - 1 >= chunk.verticalBounds().minY() &&
                    chunk.blockStateId(localX, worldY - 1, localZ) == dirt) {
                    if (const std::optional<std::int32_t> top = topMaterial(worldX, worldY - 1, worldZ, fluid);
                        top.has_value()) {
                        chunk.setBlockStateId(localX, worldY - 1, localZ, *top);
                        if (registry::blockFluids::hasFluid(*top)) {
                            chunk.markFluidUpdate(worldX, worldY - 1, worldZ);
                        }
                    }
                }
            }
        }
        index = end + 1;
    }
}

void carveShape(Chunk& chunk, const NoiseRouter& router, const CarverBiome& biomeAt) {
    NoiseChunk noiseChunk{
        router, nullptr,
        densityfunction::DensityVolume{16, router.height, 16, chunk.chunkX() * 16, router.minY, chunk.chunkZ() * 16}};
    carve(chunk, router.state->seed(), noiseChunk, router, *router.carvers, biomeAt,
          [](std::int32_t, std::int32_t, std::int32_t, bool) { return std::optional<std::int32_t>{}; });
}

} // namespace basalt::server::world::terrain::carver
