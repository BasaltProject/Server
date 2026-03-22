#include <basalt/server/world/BlockStateJson.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/carver/Carvers.hpp>
#include <basalt/server/world/terrain/material/MaterialRules.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <utility>

namespace basalt::server::world::terrain::noiseRouterData {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] std::optional<double> number(const JsonObject& object, std::string_view field) {
    const JsonValue* value = object.find(field);
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

/// parity: `Climate.Parameter.CODEC`, a single value or a `[min, max]` pair.
[[nodiscard]] std::optional<climate::Parameter> parameterOf(const JsonValue& json) {
    if (const auto* list = json.get<std::vector<JsonValue>>(); list != nullptr && list->size() == 2) {
        const auto value = [](const JsonValue& entry) -> std::optional<float> {
            if (const auto* whole = entry.get<std::int64_t>(); whole != nullptr) {
                return static_cast<float>(*whole);
            }
            const auto* real = entry.get<double>();
            return real == nullptr ? std::nullopt : std::optional{static_cast<float>(*real)};
        };
        const std::optional<float> min = value((*list)[0]);
        const std::optional<float> max = value((*list)[1]);
        if (!min.has_value() || !max.has_value()) {
            return std::nullopt;
        }
        return climate::parameterSpan(*min, *max);
    }
    if (const auto* whole = json.get<std::int64_t>(); whole != nullptr) {
        return climate::parameterPointAt(static_cast<float>(*whole));
    }
    if (const auto* real = json.get<double>(); real != nullptr) {
        return climate::parameterPointAt(static_cast<float>(*real));
    }
    return std::nullopt;
}

} // namespace

std::expected<NoiseRouter, std::string> load(std::string_view settingsId, std::int64_t seed,
                                             const densityfunction::WorldgenReader& reader) {
    const std::string id{settingsId};
    const std::optional<std::string> text =
        reader ? reader(densityfunction::resourcePath("worldgen/noise_settings", id)) : std::nullopt;
    if (!text.has_value()) {
        return std::unexpected("noise settings '" + id + "' are missing");
    }
    const std::expected<JsonValue, wire::common::JsonError> json = wire::common::parseJson(*text);
    const auto* root = json.has_value() ? json->get<JsonObject>() : nullptr;
    if (root == nullptr) {
        return std::unexpected("noise settings '" + id + "' are not a JSON object");
    }
    const auto fail = [&id](const std::string& why) { return std::unexpected("noise settings '" + id + "': " + why); };

    NoiseRouter router;
    const JsonValue* noiseField = root->find("noise");
    const auto* noise = noiseField == nullptr ? nullptr : noiseField->get<JsonObject>();
    if (noise == nullptr || !number(*noise, "min_y").has_value() || !number(*noise, "height").has_value()) {
        return fail("missing 'noise'");
    }
    router.minY = static_cast<std::int32_t>(*number(*noise, "min_y"));
    router.height = static_cast<std::int32_t>(*number(*noise, "height"));
    router.seaLevel = static_cast<std::int32_t>(number(*root, "sea_level").value_or(63.0));
    const JsonValue* legacy = root->find("legacy_random_source");
    router.legacyRandomSource = legacy != nullptr && legacy->get<bool>() != nullptr && *legacy->get<bool>();
    const JsonValue* noMobs = root->find("disable_mob_generation");
    router.disableMobGeneration = noMobs != nullptr && noMobs->get<bool>() != nullptr && *noMobs->get<bool>();
    for (const auto& [field, target] :
         {std::pair<std::string_view, std::int32_t*>{"default_block", &router.defaultBlock},
          std::pair<std::string_view, std::int32_t*>{"default_fluid", &router.defaultFluid}}) {
        const JsonValue* value = root->find(field);
        const std::optional<std::int32_t> state = value == nullptr ? std::nullopt : blockStateJson::parse(*value);
        if (!state.has_value()) {
            return fail("invalid '" + std::string{field} + "'");
        }
        *target = *state;
    }

    auto loader = std::make_shared<densityfunction::DensityFunctionLoader>(reader);
    router.state = std::make_shared<densityfunction::RandomState>(seed, router.legacyRandomSource, loader);

    const JsonValue* routerField = root->find("noise_router");
    const auto* routerObject = routerField == nullptr ? nullptr : routerField->get<JsonObject>();
    if (routerObject == nullptr) {
        return fail("missing 'noise_router'");
    }
    for (const auto& [field, target] :
         {std::pair<std::string_view, Function*>{"temperature", &router.temperature},
          std::pair<std::string_view, Function*>{"vegetation", &router.vegetation},
          std::pair<std::string_view, Function*>{"continents", &router.continents},
          std::pair<std::string_view, Function*>{"erosion", &router.erosion},
          std::pair<std::string_view, Function*>{"depth", &router.depth},
          std::pair<std::string_view, Function*>{"ridges", &router.ridges},
          std::pair<std::string_view, Function*>{"chunk_surface_level", &router.chunkSurfaceLevel},
          std::pair<std::string_view, Function*>{"final_density", &router.finalDensity}}) {
        const JsonValue* value = routerObject->find(field);
        if (value == nullptr) {
            return fail("noise_router lacks '" + std::string{field} + "'");
        }
        std::expected<Function, std::string> parsed = loader->parse(*value);
        if (!parsed.has_value()) {
            return fail("noise_router." + std::string{field} + ": " + parsed.error());
        }
        *target = std::move(*parsed);
    }

    if (const JsonValue* aquifersField = root->find("aquifers"); aquifersField != nullptr) {
        const auto* aquifers = aquifersField->get<JsonObject>();
        if (aquifers == nullptr) {
            return fail("'aquifers' is not an object");
        }
        AquiferFunctions functions;
        for (const auto& [field, target] :
             {std::pair<std::string_view, Function*>{"barrier", &functions.barrier},
              std::pair<std::string_view, Function*>{"fluid_level_floodedness", &functions.fluidLevelFloodedness},
              std::pair<std::string_view, Function*>{"fluid_level_spread", &functions.fluidLevelSpread},
              std::pair<std::string_view, Function*>{"lava", &functions.lava},
              std::pair<std::string_view, Function*>{"exclusion", &functions.exclusion},
              std::pair<std::string_view, Function*>{"surface_level", &functions.surfaceLevel}}) {
            const JsonValue* value = aquifers->find(field);
            if (value == nullptr) {
                return fail("aquifers lack '" + std::string{field} + "'");
            }
            std::expected<Function, std::string> parsed = loader->parse(*value);
            if (!parsed.has_value()) {
                return fail("aquifers." + std::string{field} + ": " + parsed.error());
            }
            *target = std::move(*parsed);
        }
        router.aquifers = std::move(functions);
    }

    material::MaterialRuleLoader rules{*loader};
    const JsonValue* materialField = root->find("material_rule");
    if (materialField == nullptr) {
        return fail("missing 'material_rule'");
    }
    auto materialRule = rules.rule(*materialField);
    if (!materialRule.has_value()) {
        return fail(materialRule.error());
    }
    router.materialRule = std::move(*materialRule);
    try {
        router.materialSystem = std::make_shared<material::MaterialSystem>(*router.state, router.defaultBlock,
                                                                           router.seaLevel, router.chunkSurfaceLevel);
    } catch (const std::exception& failure) {
        return fail(failure.what());
    }
    router.carvers = std::make_shared<carver::CarverRegistry>(reader);

    if (const JsonValue* spawnField = root->find("spawn_target"); spawnField != nullptr) {
        const auto* targets = spawnField->get<std::vector<JsonValue>>();
        if (targets == nullptr) {
            return fail("'spawn_target' is not a list");
        }
        for (const JsonValue& entry : *targets) {
            const auto* point = entry.get<JsonObject>();
            if (point == nullptr) {
                return fail("a spawn target is not an object");
            }
            SpawnTarget target;
            for (std::size_t index = 0; index < point->size(); ++index) {
                std::expected<Function, std::string> function = loader->reference(point->names()[index]);
                const std::optional<climate::Parameter> parameter = parameterOf(point->values()[index]);
                if (!function.has_value() || !parameter.has_value()) {
                    return fail("invalid spawn target entry '" + point->names()[index] + "'");
                }
                target.parameters.emplace_back(std::move(*function), *parameter);
            }
            router.spawnTarget.push_back(std::move(target));
        }
    }
    return router;
}

} // namespace basalt::server::world::terrain::noiseRouterData
