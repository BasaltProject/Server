#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

/// A whole number written either way round: the jar writes counts as integers and the discount as a
/// fraction, and a field is read for what it is rather than for how it was spelled.
[[nodiscard]] std::optional<double> numberOf(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* integer = value->get<std::int64_t>(); integer != nullptr) {
        return static_cast<double>(*integer);
    }
    if (const double* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Identifier> identifierOf(const JsonObject* object, std::string_view key) {
    if (object == nullptr) {
        return std::nullopt;
    }
    const JsonValue* value = object->find(key);
    const std::string* text = value == nullptr ? nullptr : value->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*text);
    return id.has_value() ? std::optional{*id} : std::nullopt;
}

[[nodiscard]] std::int32_t countOf(const JsonObject* object, std::int32_t fallback) {
    if (object == nullptr) {
        return fallback;
    }
    const std::optional<double> count = numberOf(object->find("count"));
    return count.has_value() ? static_cast<std::int32_t>(*count) : fallback;
}

/// parity: the `values` of a tag file, as ids. Nothing here resolves a nested tag: a level's tag
/// lists its trades outright.
[[nodiscard]] std::vector<Identifier> idsOf(std::string_view json) {
    std::vector<Identifier> ids;
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return ids;
    }
    const JsonObject* root = parsed->get<JsonObject>();
    const JsonValue* values = root == nullptr ? nullptr : root->find("values");
    const std::vector<JsonValue>* array = values == nullptr ? nullptr : values->get<std::vector<JsonValue>>();
    if (array == nullptr) {
        return ids;
    }
    for (const JsonValue& element : *array) {
        const std::string* text = element.get<std::string>();
        if (text == nullptr) {
            continue;
        }
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*text);
        if (id.has_value()) {
            ids.push_back(*id);
        }
    }
    return ids;
}

} // namespace

std::optional<VillagerTrade> RealVillagerTrades::parse(const Identifier& id, std::string_view json) {
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    const JsonObject* root = parsed->get<JsonObject>();
    if (root == nullptr) {
        return std::nullopt;
    }
    const auto objectAt = [root](std::string_view name) -> const JsonObject* {
        const JsonValue* value = root->find(name);
        return value == nullptr ? nullptr : value->get<JsonObject>();
    };
    const JsonObject* wants = objectAt("wants");
    const JsonObject* gives = objectAt("gives");
    const JsonObject* extra = objectAt("additional_wants");
    const std::optional<Identifier> wanted = identifierOf(wants, "id");
    const std::optional<Identifier> given = identifierOf(gives, "id");
    if (!wanted.has_value() || !given.has_value()) {
        return std::nullopt;
    }
    const std::optional<Identifier> alsoWanted = identifierOf(extra, "id");
    return VillagerTrade{
        .id = id,
        .wants = *wanted,
        .wantsCount = countOf(wants, 1),
        .alsoWants = alsoWanted,
        .alsoWantsCount = alsoWanted.has_value() ? countOf(extra, 1) : 0,
        .gives = *given,
        .givesCount = countOf(gives, 1),
        .maxUses = static_cast<std::int32_t>(numberOf(root->find("max_uses")).value_or(kDefaultMaxUses)),
        .experience = static_cast<std::int32_t>(numberOf(root->find("xp")).value_or(0.0)),
        .priceMultiplier = static_cast<float>(numberOf(root->find("reputation_discount")).value_or(0.0)),
    };
}

const std::vector<VillagerTrade>& RealVillagerTrades::forProfession(std::string_view profession, std::int32_t level) {
    const std::string key = std::string{profession} + "/" + std::to_string(level);
    const auto cached = cache_.find(key);
    if (cached != cache_.end()) {
        return cached->second;
    }
    std::vector<VillagerTrade> loaded;
    const std::optional<std::string> tag =
        jar_.readText ? jar_.readText("data/minecraft/tags/villager_trade/" + std::string{profession} + "/level_" +
                                      std::to_string(level) + ".json")
                      : std::nullopt;
    if (!tag.has_value()) {
        logger_.debug([&] { return "No trades for " + key + ": no such tag"; });
        return cache_.emplace(key, std::move(loaded)).first->second;
    }
    for (const Identifier& id : idsOf(*tag)) {
        const std::optional<std::string> text = jar_.readText("data/minecraft/villager_trade/" + id.path() + ".json");
        if (!text.has_value()) {
            continue;
        }
        const std::optional<VillagerTrade> trade = parse(id, *text);
        if (trade.has_value()) {
            loaded.push_back(*trade);
        }
    }
    return cache_.emplace(key, std::move(loaded)).first->second;
}

} // namespace basalt::server::runtime::world
