#include <basalt/server/world/densityfunction/DensityFunctionJson.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <utility>

namespace basalt::server::world::densityfunction {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

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

[[nodiscard]] std::string withNamespace(std::string_view id) {
    return id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
}

[[nodiscard]] std::expected<double, std::string> required(const JsonObject& object, std::string_view field) {
    const std::optional<double> value = number(object.find(field));
    if (!value.has_value()) {
        return std::unexpected("missing number '" + std::string{field} + "'");
    }
    return *value;
}

[[nodiscard]] std::optional<Axis> axisNamed(std::string_view name) {
    if (name == "x") {
        return Axis::X;
    }
    if (name == "y") {
        return Axis::Y;
    }
    if (name == "z") {
        return Axis::Z;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<UnaryType> unaryNamed(std::string_view name) {
    static constexpr std::pair<std::string_view, UnaryType> kTypes[] = {
        {"abs", UnaryType::Abs},
        {"square", UnaryType::Square},
        {"cube", UnaryType::Cube},
        {"sqrt", UnaryType::Sqrt},
        {"half_negative", UnaryType::HalfNegative},
        {"quarter_negative", UnaryType::QuarterNegative},
        {"reciprocal", UnaryType::Reciprocal},
        {"negate", UnaryType::Negate},
        {"squeeze", UnaryType::Squeeze},
        {"log", UnaryType::Log},
        {"sign", UnaryType::Sign}};
    for (const auto& [id, type] : kTypes) {
        if (id == name) {
            return type;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<BinaryType> binaryNamed(std::string_view name) {
    static constexpr std::pair<std::string_view, BinaryType> kTypes[] = {
        {"add", BinaryType::Add}, {"sub", BinaryType::Sub}, {"mul", BinaryType::Mul},
        {"div", BinaryType::Div}, {"min", BinaryType::Min}, {"max", BinaryType::Max}};
    for (const auto& [id, type] : kTypes) {
        if (id == name) {
            return type;
        }
    }
    return std::nullopt;
}

} // namespace

std::string resourcePath(std::string_view registry, std::string_view id) {
    const std::string full = withNamespace(id);
    const std::size_t colon = full.find(':');
    return full.substr(0, colon) + "/" + std::string{registry} + "/" + full.substr(colon + 1);
}

std::expected<noise::NormalNoise, std::string> parseNoiseParameters(const JsonValue& json) {
    const auto* object = json.get<JsonObject>();
    if (object == nullptr) {
        return std::unexpected(std::string{"a noise must be an object"});
    }
    noise::NormalNoise::Parameters parameters;
    const std::optional<double> baseOctave = number(object->find("base_octave"));
    if (!baseOctave.has_value()) {
        return std::unexpected(std::string{"missing 'base_octave'"});
    }
    parameters.baseOctave = static_cast<std::int32_t>(*baseOctave);
    parameters.baseAmplitude = number(object->find("base_amplitude")).value_or(1.0);
    parameters.octaveCount = static_cast<std::int32_t>(number(object->find("octave_count")).value_or(1.0));
    if (const JsonValue* normalize = object->find("normalize"); normalize != nullptr) {
        if (const auto* flag = normalize->get<bool>(); flag != nullptr) {
            parameters.normalize =
                *flag ? noise::NormalNoise::Normalization::Enabled : noise::NormalNoise::Normalization::Disabled;
        } else if (const auto* text = normalize->get<std::string>(); text != nullptr && *text == "legacy") {
            parameters.normalize = noise::NormalNoise::Normalization::Legacy;
        } else {
            return std::unexpected(std::string{"invalid 'normalize'"});
        }
    }
    if (const JsonValue* modifiers = object->find("amplitude_modifiers"); modifiers != nullptr) {
        const auto* list = modifiers->get<std::vector<JsonValue>>();
        if (list == nullptr) {
            return std::unexpected(std::string{"'amplitude_modifiers' must be a list"});
        }
        for (const JsonValue& value : *list) {
            const std::optional<double> modifier = number(&value);
            if (!modifier.has_value()) {
                return std::unexpected(std::string{"'amplitude_modifiers' holds a non-number"});
            }
            parameters.amplitudeModifiers.push_back(*modifier);
        }
        if (!parameters.amplitudeModifiers.empty() &&
            parameters.amplitudeModifiers.size() != static_cast<std::size_t>(parameters.octaveCount)) {
            return std::unexpected(std::string{"'amplitude_modifiers' does not match 'octave_count'"});
        }
    }
    return noise::NormalNoise{std::move(parameters)};
}

DensityFunctionLoader::DensityFunctionLoader(WorldgenReader reader) : reader_(std::move(reader)) {}

std::expected<std::shared_ptr<const noise::NormalNoise>, std::string>
DensityFunctionLoader::noise(std::string_view id) {
    const std::lock_guard<std::recursive_mutex> guard{mutex_};
    const std::string key = withNamespace(id);
    if (const auto found = noises_.find(key); found != noises_.end()) {
        return found->second;
    }
    const std::string path = resourcePath("worldgen/noise", key);
    const std::optional<std::string> text = reader_ ? reader_(path) : std::nullopt;
    if (!text.has_value()) {
        return std::unexpected("noise '" + key + "' is missing");
    }
    const std::expected<JsonValue, wire::common::JsonError> json = wire::common::parseJson(*text);
    if (!json.has_value()) {
        return std::unexpected("noise '" + key + "' is not valid JSON");
    }
    std::expected<noise::NormalNoise, std::string> parsed = parseNoiseParameters(*json);
    if (!parsed.has_value()) {
        return std::unexpected("noise '" + key + "': " + parsed.error());
    }
    auto held = std::make_shared<const noise::NormalNoise>(std::move(*parsed));
    noises_.emplace(key, held);
    return held;
}

std::expected<Function, std::string> DensityFunctionLoader::reference(std::string_view id) {
    const std::lock_guard<std::recursive_mutex> guard{mutex_};
    const std::string key = withNamespace(id);
    if (const auto found = functions_.find(key); found != functions_.end()) {
        return found->second;
    }
    const std::string path = resourcePath("worldgen/density_function", key);
    const std::optional<std::string> text = reader_ ? reader_(path) : std::nullopt;
    if (!text.has_value()) {
        return std::unexpected("density function '" + key + "' is missing");
    }
    const std::expected<JsonValue, wire::common::JsonError> json = wire::common::parseJson(*text);
    if (!json.has_value()) {
        return std::unexpected("density function '" + key + "' is not valid JSON");
    }
    std::expected<Function, std::string> parsed = parse(*json);
    if (!parsed.has_value()) {
        return std::unexpected("density function '" + key + "': " + parsed.error());
    }
    Function node = functions::reference(key, std::move(*parsed));
    functions_.emplace(key, node);
    return node;
}

std::expected<Function, std::string> DensityFunctionLoader::parse(const JsonValue& json) {
    if (const std::optional<double> value = number(&json); value.has_value()) {
        return functions::constant(static_cast<float>(*value));
    }
    if (const auto* id = json.get<std::string>(); id != nullptr) {
        return reference(*id);
    }
    return parseObject(json);
}

std::expected<std::shared_ptr<const Spline>, std::string> DensityFunctionLoader::parseSpline(const JsonValue& json) {
    auto spline = std::make_shared<Spline>();
    if (const std::optional<double> value = number(&json); value.has_value()) {
        spline->constant = static_cast<float>(*value);
        return spline;
    }
    const auto* object = json.get<JsonObject>();
    const JsonValue* coordinate = object == nullptr ? nullptr : object->find("coordinate");
    const JsonValue* points = object == nullptr ? nullptr : object->find("points");
    const auto* list = points == nullptr ? nullptr : points->get<std::vector<JsonValue>>();
    if (coordinate == nullptr || list == nullptr || list->empty()) {
        return std::unexpected(std::string{"a spline needs a coordinate and points"});
    }
    std::expected<Function, std::string> parsed = parse(*coordinate);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }
    spline->coordinate = std::move(*parsed);
    for (const JsonValue& entry : *list) {
        const auto* point = entry.get<JsonObject>();
        const JsonValue* value = point == nullptr ? nullptr : point->find("value");
        const std::optional<double> location = point == nullptr ? std::nullopt : number(point->find("location"));
        const std::optional<double> derivative = point == nullptr ? std::nullopt : number(point->find("derivative"));
        if (value == nullptr || !location.has_value() || !derivative.has_value()) {
            return std::unexpected(std::string{"a spline point needs location, value and derivative"});
        }
        std::expected<std::shared_ptr<const Spline>, std::string> child = parseSpline(*value);
        if (!child.has_value()) {
            return std::unexpected(child.error());
        }
        spline->points.push_back({static_cast<float>(*location), std::move(*child), static_cast<float>(*derivative)});
    }
    return spline;
}

std::expected<Function, std::string> DensityFunctionLoader::parseObject(const JsonValue& json) {
    const auto* object = json.get<JsonObject>();
    const JsonValue* typeField = object == nullptr ? nullptr : object->find("type");
    const auto* typeName = typeField == nullptr ? nullptr : typeField->get<std::string>();
    if (typeName == nullptr) {
        return std::unexpected(std::string{"a density function needs a type"});
    }
    std::string_view type{*typeName};
    if (type.starts_with("minecraft:")) {
        type.remove_prefix(10);
    }
    const auto child = [this, object](std::string_view field) -> std::expected<Function, std::string> {
        const JsonValue* value = object->find(field);
        if (value == nullptr) {
            return std::unexpected("missing '" + std::string{field} + "'");
        }
        return parse(*value);
    };
    const auto optionalChild = [this, object](std::string_view field) -> std::expected<Function, std::string> {
        const JsonValue* value = object->find(field);
        return value == nullptr ? std::expected<Function, std::string>{functions::zero()} : parse(*value);
    };
    const auto noiseId = [object]() -> std::optional<std::string> {
        const JsonValue* value = object->find("noise");
        const auto* id = value == nullptr ? nullptr : value->get<std::string>();
        return id == nullptr ? std::nullopt : std::optional{withNamespace(*id)};
    };
#define BASALT_TRY(name, expr)                                                                                         \
    auto name = (expr);                                                                                                \
    if (!name.has_value()) {                                                                                           \
        return std::unexpected(std::string{type} + ": " + name.error());                                               \
    }

    if (type == "constant") {
        BASALT_TRY(value, required(*object, "value"));
        return functions::constant(static_cast<float>(*value));
    }
    if (type == "blend_alpha") {
        return functions::simple(SimpleType::BlendAlpha);
    }
    if (type == "blend_offset") {
        return functions::simple(SimpleType::BlendOffset);
    }
    if (type == "beardifier") {
        return functions::simple(SimpleType::Beardifier);
    }
    if (type == "end_outer_islands") {
        return functions::endIslands();
    }
    if (type == "noise") {
        const std::optional<std::string> id = noiseId();
        if (!id.has_value()) {
            return std::unexpected(std::string{"noise: 'noise' must name a noise"});
        }
        BASALT_TRY(parameters, noise(*id));
        BASALT_TRY(xzScale, required(*object, "xz_scale"));
        BASALT_TRY(yScale, required(*object, "y_scale"));
        BASALT_TRY(shiftX, optionalChild("shift_x"));
        BASALT_TRY(shiftY, optionalChild("shift_y"));
        BASALT_TRY(shiftZ, optionalChild("shift_z"));
        return functions::noise(*id, *parameters, *xzScale, *yScale, *shiftX, *shiftY, *shiftZ);
    }
    if (type == "shift_a" || type == "shift_b" || type == "shift") {
        const std::optional<std::string> id = noiseId();
        if (!id.has_value()) {
            return std::unexpected(std::string{type} + ": 'noise' must name a noise");
        }
        BASALT_TRY(parameters, noise(*id));
        const ShiftType shiftType =
            type == "shift_a" ? ShiftType::ShiftA : (type == "shift_b" ? ShiftType::ShiftB : ShiftType::Shift);
        return functions::shift(shiftType, *id, *parameters);
    }
    if (type == "distance_to_point") {
        const JsonValue* point = object->find("point");
        const auto* list = point == nullptr ? nullptr : point->get<std::vector<JsonValue>>();
        const JsonValue* metricField = object->find("metric");
        const auto* metric = metricField == nullptr ? nullptr : metricField->get<std::string>();
        if (list == nullptr || list->size() != 3 || metric == nullptr) {
            return std::unexpected(std::string{"distance_to_point needs a point and a metric"});
        }
        DistanceMetric parsedMetric = DistanceMetric::Euclidean;
        if (*metric == "euclidean_squared") {
            parsedMetric = DistanceMetric::EuclideanSquared;
        } else if (*metric == "manhattan") {
            parsedMetric = DistanceMetric::Manhattan;
        } else if (*metric == "chebyshev") {
            parsedMetric = DistanceMetric::Chebyshev;
        } else if (*metric != "euclidean") {
            return std::unexpected("distance_to_point: unknown metric '" + *metric + "'");
        }
        return functions::distanceToPoint(static_cast<std::int32_t>(number(&(*list)[0]).value_or(0.0)),
                                          static_cast<std::int32_t>(number(&(*list)[1]).value_or(0.0)),
                                          static_cast<std::int32_t>(number(&(*list)[2]).value_or(0.0)), parsedMetric);
    }
    if (type == "gradient") {
        const JsonValue* axisField = object->find("axis");
        const auto* axisName = axisField == nullptr ? nullptr : axisField->get<std::string>();
        const std::optional<Axis> axis = axisName == nullptr ? std::nullopt : axisNamed(*axisName);
        if (!axis.has_value()) {
            return std::unexpected(std::string{"gradient: invalid axis"});
        }
        Tiling tiling = Tiling::ClampToEdge;
        if (const JsonValue* tilingField = object->find("tiling"); tilingField != nullptr) {
            const auto* name = tilingField->get<std::string>();
            if (name != nullptr && *name == "repeat") {
                tiling = Tiling::Repeat;
            } else if (name != nullptr && *name == "mirrored_repeat") {
                tiling = Tiling::MirroredRepeat;
            } else if (name == nullptr || *name != "clamp_to_edge") {
                return std::unexpected(std::string{"gradient: invalid tiling"});
            }
        }
        BASALT_TRY(from, required(*object, "from_coordinate"));
        BASALT_TRY(to, required(*object, "to_coordinate"));
        BASALT_TRY(fromValue, required(*object, "from_value"));
        BASALT_TRY(toValue, required(*object, "to_value"));
        return functions::gradient(*axis, tiling, static_cast<std::int32_t>(*from), static_cast<std::int32_t>(*to),
                                   static_cast<float>(*fromValue), static_cast<float>(*toValue));
    }
    if (const std::optional<UnaryType> unary = unaryNamed(type); unary.has_value()) {
        BASALT_TRY(input, child("input"));
        return functions::unary(*unary, *input);
    }
    if (const std::optional<BinaryType> binary = binaryNamed(type); binary.has_value()) {
        BASALT_TRY(left, child("left"));
        BASALT_TRY(right, child("right"));
        return functions::binary(*binary, *left, *right);
    }
    if (type == "spline") {
        const JsonValue* field = object->find("spline");
        if (field == nullptr) {
            return std::unexpected(std::string{"spline: missing 'spline'"});
        }
        BASALT_TRY(spline, parseSpline(*field));
        return functions::spline(*spline);
    }
    if (type == "lerp") {
        BASALT_TRY(alpha, child("alpha"));
        BASALT_TRY(first, child("first"));
        BASALT_TRY(second, child("second"));
        return functions::lerp(*alpha, *first, *second);
    }
    if (type == "clamp") {
        BASALT_TRY(input, child("input"));
        BASALT_TRY(min, required(*object, "min"));
        BASALT_TRY(max, required(*object, "max"));
        return functions::clamp(*input, static_cast<float>(*min), static_cast<float>(*max));
    }
    if (type == "range_choice") {
        BASALT_TRY(input, child("input"));
        BASALT_TRY(min, required(*object, "min_inclusive"));
        BASALT_TRY(max, required(*object, "max_exclusive"));
        BASALT_TRY(inRange, child("when_in_range"));
        BASALT_TRY(outOfRange, child("when_out_of_range"));
        return functions::rangeChoice(*input, static_cast<float>(*min), static_cast<float>(*max), *inRange,
                                      *outOfRange);
    }
    if (type == "interval_select") {
        BASALT_TRY(input, child("input"));
        const JsonValue* thresholdsField = object->find("thresholds");
        const JsonValue* functionsField = object->find("functions");
        const auto* thresholds = thresholdsField == nullptr ? nullptr : thresholdsField->get<std::vector<JsonValue>>();
        const auto* list = functionsField == nullptr ? nullptr : functionsField->get<std::vector<JsonValue>>();
        if (thresholds == nullptr || list == nullptr || list->size() < 2 || thresholds->size() + 1 != list->size()) {
            return std::unexpected(std::string{"interval_select needs n thresholds for n + 1 functions"});
        }
        std::vector<float> parsedThresholds;
        for (const JsonValue& value : *thresholds) {
            parsedThresholds.push_back(static_cast<float>(number(&value).value_or(0.0)));
        }
        std::vector<Function> parsedFunctions;
        for (const JsonValue& value : *list) {
            BASALT_TRY(function, parse(value));
            parsedFunctions.push_back(*function);
        }
        return functions::intervalSelect(*input, std::move(parsedThresholds), std::move(parsedFunctions));
    }
    if (type == "cache") {
        BASALT_TRY(input, child("input"));
        return functions::cache(*input);
    }
    if (type == "blend_density") {
        BASALT_TRY(input, child("input"));
        return functions::blendDensity(*input);
    }
    if (type == "interpolated") {
        BASALT_TRY(input, child("input"));
        BASALT_TRY(cellXz, required(*object, "cell_size_xz"));
        BASALT_TRY(cellY, required(*object, "cell_size_y"));
        return functions::interpolated(*input, static_cast<std::int32_t>(*cellXz), static_cast<std::int32_t>(*cellY));
    }
    if (type == "slice") {
        const JsonValue* axisField = object->find("axis");
        const auto* axisName = axisField == nullptr ? nullptr : axisField->get<std::string>();
        const std::optional<Axis> axis = axisName == nullptr ? std::nullopt : axisNamed(*axisName);
        if (!axis.has_value()) {
            return std::unexpected(std::string{"slice: invalid axis"});
        }
        BASALT_TRY(coordinate, required(*object, "coordinate"));
        BASALT_TRY(input, child("input"));
        return functions::slice(*axis, static_cast<std::int32_t>(*coordinate), *input);
    }
    if (type == "find_top_surface") {
        BASALT_TRY(density, child("density"));
        BASALT_TRY(upper, child("upper_bound"));
        BASALT_TRY(lower, required(*object, "lower_bound"));
        BASALT_TRY(cellHeight, required(*object, "cell_height"));
        return functions::findTopSurface(*density, *upper, static_cast<std::int32_t>(*lower),
                                         static_cast<std::int32_t>(*cellHeight));
    }
    if (type == "old_blended_noise") {
        BASALT_TRY(xzScale, required(*object, "xz_scale"));
        BASALT_TRY(yScale, required(*object, "y_scale"));
        BASALT_TRY(xzFactor, required(*object, "xz_factor"));
        BASALT_TRY(yFactor, required(*object, "y_factor"));
        BASALT_TRY(smear, required(*object, "smear_scale_multiplier"));
        return functions::blendedNoise(*xzScale, *yScale, *xzFactor, *yFactor, *smear);
    }
#undef BASALT_TRY
    return std::unexpected("unsupported density function type '" + std::string{type} + "'");
}

} // namespace basalt::server::world::densityfunction
