#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>
#include <expected>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] std::optional<std::int64_t> integerOf(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* number = value->get<std::int64_t>(); number != nullptr) {
        return *number;
    }
    const double* real = value->get<double>();
    return real == nullptr ? std::nullopt : std::optional{static_cast<std::int64_t>(*real)};
}

[[nodiscard]] std::optional<double> realOf(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const double* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    const std::int64_t* number = value->get<std::int64_t>();
    return number == nullptr ? std::nullopt : std::optional{static_cast<double>(*number)};
}

[[nodiscard]] std::vector<SpawnerEntry> entriesOf(const std::vector<JsonValue>& list) {
    std::vector<SpawnerEntry> entries;
    for (const JsonValue& element : list) {
        const JsonObject* fields = element.get<JsonObject>();
        if (fields == nullptr) {
            continue;
        }
        const JsonValue* type = fields->find("type");
        const std::string* named = type == nullptr ? nullptr : type->get<std::string>();
        const std::optional<std::int64_t> weight = integerOf(fields->find("weight"));
        const std::optional<std::int64_t> minCount = integerOf(fields->find("minCount"));
        const std::optional<std::int64_t> maxCount = integerOf(fields->find("maxCount"));
        if (named == nullptr || !weight.has_value() || !minCount.has_value() || !maxCount.has_value()) {
            continue;
        }
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(*named);
        if (!parsed.has_value()) {
            continue;
        }
        entries.push_back(SpawnerEntry{.type = *parsed,
                                       .weight = static_cast<std::int32_t>(*weight),
                                       .minCount = static_cast<std::int32_t>(*minCount),
                                       .maxCount = static_cast<std::int32_t>(*maxCount)});
    }
    return entries;
}

} // namespace

RealBiomeSpawns::RealBiomeSpawns(Jar jar, wire::common::Logger logger)
    : jar_(std::move(jar)), logger_(std::move(logger)) {}

RealBiomeSpawns::Settings RealBiomeSpawns::parse(std::string_view json) {
    Settings settings;
    const std::expected<JsonValue, wire::common::JsonError> document = wire::common::parseJson(json);
    if (!document.has_value()) {
        return settings;
    }
    const JsonObject* root = document->get<JsonObject>();
    if (root == nullptr) {
        return settings;
    }

    if (const JsonValue* spawners = root->find("spawners"); spawners != nullptr) {
        if (const JsonObject* lists = spawners->get<JsonObject>(); lists != nullptr) {
            for (std::size_t index = 0; index < lists->names().size(); index++) {
                const std::vector<JsonValue>* list = lists->values()[index].get<std::vector<JsonValue>>();
                if (list != nullptr) {
                    settings.spawners.emplace(lists->names()[index], entriesOf(*list));
                }
            }
        }
    }

    if (const std::optional<double> probability = realOf(root->find("creature_spawn_probability"));
        probability.has_value()) {
        settings.creatureProbability = static_cast<float>(*probability);
    }

    if (const JsonValue* costs = root->find("spawn_costs"); costs != nullptr) {
        if (const JsonObject* named = costs->get<JsonObject>(); named != nullptr) {
            for (std::size_t index = 0; index < named->names().size(); index++) {
                const JsonObject* fields = named->values()[index].get<JsonObject>();
                if (fields == nullptr) {
                    continue;
                }
                const std::optional<double> energyBudget = realOf(fields->find("energy_budget"));
                const std::optional<double> charge = realOf(fields->find("charge"));
                const std::expected<Identifier, wire::common::IdentifierError> type =
                    Identifier::parse(named->names()[index]);
                if (!type.has_value() || !energyBudget.has_value() || !charge.has_value()) {
                    continue;
                }
                settings.costs.emplace(*type, MobSpawnCost{.energyBudget = *energyBudget, .charge = *charge});
            }
        }
    }
    return settings;
}

const RealBiomeSpawns::Settings& RealBiomeSpawns::settingsOf(const Identifier& biome) const {
    const std::lock_guard<std::mutex> guard{lock_};
    const std::string key{biome.path()};
    const auto cached = cache_.find(key);
    if (cached != cache_.end()) {
        return cached->second;
    }
    Settings settings;
    const std::optional<std::string> json =
        jar_.readText ? jar_.readText("data/minecraft/worldgen/biome/" + key + ".json") : std::nullopt;
    if (json.has_value()) {
        settings = parse(*json);
    } else {
        logger_.debug([&key] { return "No spawn list for biome '" + key + "'"; });
    }
    return cache_.emplace(key, std::move(settings)).first->second;
}

std::vector<SpawnerEntry> RealBiomeSpawns::listFor(const Identifier& biome, std::string_view category) const {
    const Settings& settings = settingsOf(biome);
    const auto found = settings.spawners.find(category);
    return found == settings.spawners.end() ? std::vector<SpawnerEntry>{} : found->second;
}

float RealBiomeSpawns::creatureProbabilityFor(const Identifier& biome) const {
    return settingsOf(biome).creatureProbability;
}

std::optional<MobSpawnCost> RealBiomeSpawns::costFor(const Identifier& biome, const Identifier& type) const {
    const Settings& settings = settingsOf(biome);
    const auto found = settings.costs.find(type);
    return found == settings.costs.end() ? std::nullopt : std::optional{found->second};
}

RealBiomeSpawns::Settings RealBiomeSpawns::of(const Identifier& biome) const { return settingsOf(biome); }

} // namespace basalt::server::runtime::world
