#include <basalt/server/runtime/world/NoiseParametersJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>

namespace basalt::server::runtime::world::noiseParametersJson {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

/// A JSON number, whichever of the two shapes it was written in.
[[nodiscard]] std::optional<double> numberOf(const JsonValue& value) {
    if (const auto* whole = value.get<std::int64_t>(); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    const auto* real = value.get<double>();
    return real == nullptr ? std::nullopt : std::optional{*real};
}

} // namespace

std::optional<server::world::noise::NormalNoise::Parameters> parse(std::string_view json) {
    const std::expected<JsonValue, wire::common::JsonError> root = wire::common::parseJson(json);
    const auto* object = root.has_value() ? root->get<JsonObject>() : nullptr;
    if (object == nullptr) {
        return std::nullopt;
    }
    const JsonValue* firstOctave = object->find("firstOctave");
    const JsonValue* amplitudes = object->find("amplitudes");
    const auto* octave = firstOctave == nullptr ? nullptr : firstOctave->get<std::int64_t>();
    const auto* list = amplitudes == nullptr ? nullptr : amplitudes->get<std::vector<JsonValue>>();
    if (octave == nullptr || list == nullptr) {
        return std::nullopt;
    }
    server::world::noise::NormalNoise::Parameters parameters{.firstOctave = static_cast<std::int32_t>(*octave),
                                                             .amplitudes = {}};
    parameters.amplitudes.reserve(list->size());
    for (const JsonValue& value : *list) {
        const std::optional<double> amplitude = numberOf(value);
        if (!amplitude.has_value()) {
            return std::nullopt;
        }
        parameters.amplitudes.push_back(*amplitude);
    }
    return parameters;
}

} // namespace basalt::server::runtime::world::noiseParametersJson
