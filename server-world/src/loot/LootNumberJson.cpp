#include "LootJsonParts.hpp"

#include <basalt/wire/common/JavaCast.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot::lootTableJson::parts {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

using IntKind = IntProvider::Kind;
using FloatKind = FloatProvider::Kind;

/// parity: the names `ContextIntProviderTypes` registers the operators under.
[[nodiscard]] const std::map<std::string, IntKind, std::less<>>& intOperators() {
    static const std::map<std::string, IntKind, std::less<>> kOperators{
        {"minecraft:add", IntKind::Sum},       {"minecraft:mul", IntKind::Product},
        {"minecraft:min", IntKind::Minimum},   {"minecraft:max", IntKind::Maximum},
        {"minecraft:avg", IntKind::Average},   {"minecraft:sub", IntKind::Difference},
        {"minecraft:div", IntKind::Quotient},  {"minecraft:floor_div", IntKind::FloorQuotient},
        {"minecraft:mod", IntKind::Modulus},   {"minecraft:floor_mod", IntKind::FloorModulus},
        {"minecraft:negate", IntKind::Negate}, {"minecraft:abs", IntKind::Absolute},
    };
    return kOperators;
}

/// parity: the names `ContextFloatProviderTypes` registers the operators under.
[[nodiscard]] const std::map<std::string, FloatKind, std::less<>>& floatOperators() {
    static const std::map<std::string, FloatKind, std::less<>> kOperators{
        {"minecraft:add", FloatKind::Sum},           {"minecraft:mul", FloatKind::Product},
        {"minecraft:min", FloatKind::Minimum},       {"minecraft:max", FloatKind::Maximum},
        {"minecraft:avg", FloatKind::Average},       {"minecraft:length", FloatKind::Length},
        {"minecraft:sub", FloatKind::Difference},    {"minecraft:div", FloatKind::Quotient},
        {"minecraft:mod", FloatKind::Modulus},       {"minecraft:negate", FloatKind::Negate},
        {"minecraft:abs", FloatKind::Absolute},      {"minecraft:floor", FloatKind::Floor},
        {"minecraft:ceil", FloatKind::Ceiling},      {"minecraft:round", FloatKind::Round},
        {"minecraft:truncate", FloatKind::Truncate}, {"minecraft:sqrt", FloatKind::SquareRoot},
        {"minecraft:sin", FloatKind::Sine},          {"minecraft:cos", FloatKind::Cosine},
    };
    return kOperators;
}

[[nodiscard]] bool isAggregate(IntKind kind) {
    return kind == IntKind::Sum || kind == IntKind::Product || kind == IntKind::Minimum || kind == IntKind::Maximum ||
           kind == IntKind::Average;
}

[[nodiscard]] bool isAggregate(FloatKind kind) {
    return kind == FloatKind::Sum || kind == FloatKind::Product || kind == FloatKind::Minimum ||
           kind == FloatKind::Maximum || kind == FloatKind::Average || kind == FloatKind::Length;
}

/// parity: `Codec.INT` over a JSON number, `Number.intValue`, which truncates a fraction.
[[nodiscard]] std::optional<std::int32_t> inlineInt(const JsonValue& value) {
    if (const std::int64_t* whole = value.get<std::int64_t>(); whole != nullptr) {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(*whole)));
    }
    if (const double* real = value.get<double>(); real != nullptr) {
        return wire::common::toIntLikeJava(*real);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<float> inlineFloat(const JsonValue& value) {
    if (const std::int64_t* whole = value.get<std::int64_t>(); whole != nullptr) {
        return static_cast<float>(*whole);
    }
    if (const double* real = value.get<double>(); real != nullptr) {
        return static_cast<float>(*real);
    }
    return std::nullopt;
}

/// parity: `ScoreboardNameProviders.CODEC`: an entity target by name, or `fixed` and `context`.
[[nodiscard]] std::optional<ScoreTarget> scoreTargetOf(const JsonValue* value) {
    static const std::map<std::string, EntityTarget, std::less<>> kTargets{
        {"this", EntityTarget::This},
        {"attacker", EntityTarget::Attacker},
        {"direct_attacker", EntityTarget::DirectAttacker},
        {"attacking_player", EntityTarget::AttackingPlayer},
        {"target_entity", EntityTarget::TargetEntity},
        {"interacting_entity", EntityTarget::InteractingEntity},
    };
    std::optional<std::string> target = asString(value);
    if (const JsonObject* typed = asObject(value); typed != nullptr) {
        if (typeOf(*typed) == std::optional<std::string>{"minecraft:fixed"}) {
            const std::optional<std::string> name = asString(typed->find("name"));
            return name.has_value() ? std::optional{ScoreTarget{.name = *name}} : std::nullopt;
        }
        target = asString(typed->find("target"));
    }
    const auto found = target.has_value() ? kTargets.find(*target) : kTargets.end();
    return found == kTargets.end() ? std::nullopt : std::optional{ScoreTarget{.entity = found->second}};
}

[[nodiscard]] std::optional<wire::common::Identifier> identifierAt(const JsonObject& object, std::string_view key) {
    const std::optional<std::string> text = asString(object.find(key));
    if (!text.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(*text);
    return id.has_value() ? std::optional{*id} : std::nullopt;
}

} // namespace

Parsed<IntProvider> Reader::intProvider(const JsonValue& value) {
    if (const std::optional<std::int32_t> constant = inlineInt(value); constant.has_value()) {
        return IntProvider::constant(*constant);
    }
    if (const std::optional<std::string> id = asString(&value); id.has_value()) {
        const std::string key = "context_int_provider/" + *id;
        if (references_ != nullptr) {
            if (std::optional<LootReferences::Entry> known = references_->cached(key); known.has_value()) {
                return std::get<IntProvider>(*known);
            }
        }
        Parsed<JsonValue> json = named("context_int_provider", *id);
        if (!json.has_value()) {
            return std::unexpected(json.error());
        }
        Parsed<IntProvider> resolved = intProvider(*json);
        resolving_.pop_back();
        if (resolved.has_value()) {
            finish(key, *resolved);
        }
        return resolved;
    }
    const JsonObject* object = value.get<JsonObject>();
    if (object == nullptr) {
        return std::unexpected(missing("int provider"));
    }
    return typedInt(*object);
}

Parsed<IntProvider> Reader::typedInt(const JsonObject& object) {
    const std::optional<std::string> type = typeOf(object);
    if (!type.has_value()) {
        return std::unexpected(missing("int provider type"));
    }
    const auto field = [&](std::string_view key) -> Parsed<IntProvider> {
        const JsonValue* value = object.find(key);
        return value == nullptr ? std::unexpected(missing(*type + " " + std::string{key})) : intProvider(*value);
    };
    const auto optionalField = [&](std::string_view key) -> Parsed<IntProvider> {
        const JsonValue* value = object.find(key);
        return value == nullptr ? Parsed<IntProvider>{IntProvider::constant(0)} : intProvider(*value);
    };
    if (*type == "minecraft:constant") {
        const JsonValue* value = object.find("value");
        const std::optional<std::int32_t> constant = value == nullptr ? std::nullopt : inlineInt(*value);
        return constant.has_value() ? Parsed<IntProvider>{IntProvider::constant(*constant)}
                                    : std::unexpected(missing("constant value"));
    }
    if (*type == "minecraft:uniform" || *type == "minecraft:pow") {
        const bool uniform = *type == "minecraft:uniform";
        Parsed<IntProvider> low = field(uniform ? "min" : "base");
        if (!low.has_value()) {
            return low;
        }
        Parsed<IntProvider> high = field(uniform ? "max" : "exponent");
        if (!high.has_value()) {
            return high;
        }
        return uniform ? IntProvider::uniform(std::move(*low), std::move(*high))
                       : IntProvider::binary(IntKind::Power, std::move(*low), std::move(*high));
    }
    if (*type == "minecraft:binomial") {
        Parsed<IntProvider> tries = field("n");
        if (!tries.has_value()) {
            return tries;
        }
        const JsonValue* chance = object.find("p");
        Parsed<FloatProvider> p = chance == nullptr ? std::unexpected(missing("binomial p")) : floatProvider(*chance);
        if (!p.has_value()) {
            return std::unexpected(p.error());
        }
        return IntProvider::binomial(std::move(*tries), std::move(*p));
    }
    if (const auto found = intOperators().find(*type); found != intOperators().end()) {
        const IntKind kind = found->second;
        if (isAggregate(kind)) {
            const std::vector<JsonValue>* inputs = asArray(object.find("inputs"));
            if (inputs == nullptr || inputs->empty()) {
                return std::unexpected(missing(*type + " inputs"));
            }
            std::vector<IntProvider> parsed;
            for (const JsonValue& input : *inputs) {
                Parsed<IntProvider> one = intProvider(input);
                if (!one.has_value()) {
                    return one;
                }
                parsed.push_back(std::move(*one));
            }
            return IntProvider::aggregate(kind, std::move(parsed));
        }
        if (kind == IntKind::Negate || kind == IntKind::Absolute) {
            Parsed<IntProvider> input = field("input");
            return input.has_value() ? Parsed<IntProvider>{IntProvider::unary(kind, std::move(*input))} : input;
        }
        Parsed<IntProvider> left = field("left");
        if (!left.has_value()) {
            return left;
        }
        Parsed<IntProvider> right = field("right");
        if (!right.has_value()) {
            return right;
        }
        return IntProvider::binary(kind, std::move(*left), std::move(*right));
    }
    if (*type == "minecraft:from_float") {
        const JsonValue* input = object.find("input");
        Parsed<FloatProvider> parsed =
            input == nullptr ? std::unexpected(missing("from_float input")) : floatProvider(*input);
        return parsed.has_value() ? Parsed<IntProvider>{IntProvider::fromFloat(std::move(*parsed))}
                                  : std::unexpected(parsed.error());
    }
    if (*type == "minecraft:conditional") {
        const JsonValue* test = object.find("condition");
        Parsed<LootCondition> gate =
            test == nullptr ? std::unexpected(missing("conditional condition")) : condition(*test);
        if (!gate.has_value()) {
            return std::unexpected(gate.error());
        }
        Parsed<IntProvider> onTrue = field("on_true");
        if (!onTrue.has_value()) {
            return onTrue;
        }
        Parsed<IntProvider> onFalse = optionalField("on_false");
        if (!onFalse.has_value()) {
            return onFalse;
        }
        return IntProvider::conditional(std::move(*gate), std::move(*onTrue), std::move(*onFalse));
    }
    if (*type == "minecraft:number_dispatcher") {
        const std::vector<JsonValue>* cases = asArray(object.find("cases"));
        if (cases == nullptr) {
            return std::unexpected(missing("number_dispatcher cases"));
        }
        std::vector<std::pair<LootCondition, IntProvider>> parsed;
        for (const JsonValue& each : *cases) {
            const JsonObject* one = each.get<JsonObject>();
            const JsonValue* test = one == nullptr ? nullptr : one->find("condition");
            const JsonValue* value = one == nullptr ? nullptr : one->find("value");
            if (test == nullptr || value == nullptr) {
                return std::unexpected(missing("number_dispatcher case"));
            }
            Parsed<LootCondition> gate = condition(*test);
            if (!gate.has_value()) {
                return std::unexpected(gate.error());
            }
            Parsed<IntProvider> result = intProvider(*value);
            if (!result.has_value()) {
                return result;
            }
            parsed.emplace_back(std::move(*gate), std::move(*result));
        }
        Parsed<IntProvider> fallback = optionalField("default");
        if (!fallback.has_value()) {
            return fallback;
        }
        return IntProvider::dispatcher(std::move(parsed), std::move(*fallback));
    }
    if (*type == "minecraft:weighted_list") {
        const std::vector<JsonValue>* distribution = asArray(object.find("distribution"));
        if (distribution == nullptr || distribution->empty()) {
            return std::unexpected(missing("weighted_list distribution"));
        }
        std::vector<std::pair<IntProvider, std::int32_t>> parsed;
        for (const JsonValue& each : *distribution) {
            const JsonObject* one = each.get<JsonObject>();
            const JsonValue* data = one == nullptr ? nullptr : one->find("data");
            const std::optional<double> weight = one == nullptr ? std::nullopt : asNumber(one->find("weight"));
            if (data == nullptr || !weight.has_value()) {
                return std::unexpected(missing("weighted_list entry"));
            }
            Parsed<IntProvider> value = intProvider(*data);
            if (!value.has_value()) {
                return value;
            }
            parsed.emplace_back(std::move(*value), static_cast<std::int32_t>(*weight));
        }
        return IntProvider::weightedList(std::move(parsed));
    }
    if (*type == "minecraft:score") {
        const std::optional<ScoreTarget> target = scoreTargetOf(object.find("target"));
        const std::optional<std::string> score = asString(object.find("score"));
        if (!target.has_value() || !score.has_value()) {
            return std::unexpected(missing("score target"));
        }
        Parsed<IntProvider> fallback = optionalField("fallback");
        return fallback.has_value() ? Parsed<IntProvider>{IntProvider::score(*target, *score, std::move(*fallback))}
                                    : fallback;
    }
    if (*type == "minecraft:storage") {
        const std::optional<wire::common::Identifier> storage = identifierAt(object, "storage");
        const std::optional<std::string> path = asString(object.find("path"));
        if (!storage.has_value() || !path.has_value()) {
            return std::unexpected(missing("storage access"));
        }
        Parsed<IntProvider> fallback = optionalField("fallback");
        return fallback.has_value() ? Parsed<IntProvider>{IntProvider::storage(*storage, *path, std::move(*fallback))}
                                    : fallback;
    }
    if (*type == "minecraft:environment_attribute") {
        const std::optional<wire::common::Identifier> attribute = identifierAt(object, "attribute");
        return attribute.has_value() ? Parsed<IntProvider>{IntProvider::environmentAttribute(*attribute)}
                                     : std::unexpected(missing("environment attribute"));
    }
    return std::unexpected(LootTableParseError{"Unsupported number provider type: " + *type});
}

Parsed<FloatProvider> Reader::floatProvider(const JsonValue& value) {
    if (const std::optional<float> constant = inlineFloat(value); constant.has_value()) {
        return FloatProvider::constant(*constant);
    }
    if (const std::optional<std::string> id = asString(&value); id.has_value()) {
        const std::string key = "context_float_provider/" + *id;
        if (references_ != nullptr) {
            if (std::optional<LootReferences::Entry> known = references_->cached(key); known.has_value()) {
                return std::get<FloatProvider>(*known);
            }
        }
        Parsed<JsonValue> json = named("context_float_provider", *id);
        if (!json.has_value()) {
            return std::unexpected(json.error());
        }
        Parsed<FloatProvider> resolved = floatProvider(*json);
        resolving_.pop_back();
        if (resolved.has_value()) {
            finish(key, *resolved);
        }
        return resolved;
    }
    const JsonObject* object = value.get<JsonObject>();
    if (object == nullptr) {
        return std::unexpected(missing("float provider"));
    }
    return typedFloat(*object);
}

Parsed<FloatProvider> Reader::typedFloat(const JsonObject& object) {
    const std::optional<std::string> type = typeOf(object);
    if (!type.has_value()) {
        return std::unexpected(missing("float provider type"));
    }
    if (const auto found = floatOperators().find(*type); found != floatOperators().end()) {
        const FloatKind kind = found->second;
        if (isAggregate(kind)) {
            const std::vector<JsonValue>* inputs = asArray(object.find("inputs"));
            if (inputs == nullptr || inputs->empty()) {
                return std::unexpected(missing(*type + " inputs"));
            }
            std::vector<FloatProvider> parsed;
            for (const JsonValue& input : *inputs) {
                Parsed<FloatProvider> one = floatProvider(input);
                if (!one.has_value()) {
                    return one;
                }
                parsed.push_back(std::move(*one));
            }
            return FloatProvider::aggregate(kind, std::move(parsed));
        }
        if (kind == FloatKind::Difference || kind == FloatKind::Quotient || kind == FloatKind::Modulus) {
            const JsonValue* left = object.find("left");
            const JsonValue* right = object.find("right");
            if (left == nullptr || right == nullptr) {
                return std::unexpected(missing(*type + " operands"));
            }
            Parsed<FloatProvider> first = floatProvider(*left);
            if (!first.has_value()) {
                return first;
            }
            Parsed<FloatProvider> second = floatProvider(*right);
            if (!second.has_value()) {
                return second;
            }
            return FloatProvider::binary(kind, std::move(*first), std::move(*second));
        }
        const JsonValue* input = object.find("input");
        Parsed<FloatProvider> parsed =
            input == nullptr ? std::unexpected(missing(*type + " input")) : floatProvider(*input);
        return parsed.has_value() ? Parsed<FloatProvider>{FloatProvider::unary(kind, std::move(*parsed))} : parsed;
    }
    return typedFloatOps(*type, object);
}

Parsed<FloatProvider> Reader::typedFloatOps(const std::string& type, const JsonObject& object) {
    const auto field = [&](std::string_view key) -> Parsed<FloatProvider> {
        const JsonValue* value = object.find(key);
        return value == nullptr ? std::unexpected(missing(type + " " + std::string{key})) : floatProvider(*value);
    };
    const auto optionalField = [&](std::string_view key) -> Parsed<FloatProvider> {
        const JsonValue* value = object.find(key);
        return value == nullptr ? Parsed<FloatProvider>{FloatProvider::constant(0.0F)} : floatProvider(*value);
    };
    if (type == "minecraft:constant") {
        const JsonValue* value = object.find("value");
        const std::optional<float> constant = value == nullptr ? std::nullopt : inlineFloat(*value);
        return constant.has_value() ? Parsed<FloatProvider>{FloatProvider::constant(*constant)}
                                    : std::unexpected(missing("constant value"));
    }
    if (type == "minecraft:uniform" || type == "minecraft:pow") {
        const bool uniform = type == "minecraft:uniform";
        Parsed<FloatProvider> low = field(uniform ? "min" : "base");
        if (!low.has_value()) {
            return low;
        }
        Parsed<FloatProvider> high = field(uniform ? "max" : "exponent");
        if (!high.has_value()) {
            return high;
        }
        return uniform ? FloatProvider::uniform(std::move(*low), std::move(*high))
                       : FloatProvider::binary(FloatKind::Power, std::move(*low), std::move(*high));
    }
    if (type == "minecraft:from_int") {
        const JsonValue* input = object.find("input");
        Parsed<IntProvider> parsed =
            input == nullptr ? std::unexpected(missing("from_int input")) : intProvider(*input);
        return parsed.has_value() ? Parsed<FloatProvider>{FloatProvider::fromInt(std::move(*parsed))}
                                  : std::unexpected(parsed.error());
    }
    if (type == "minecraft:conditional") {
        const JsonValue* test = object.find("condition");
        Parsed<LootCondition> gate =
            test == nullptr ? std::unexpected(missing("conditional condition")) : condition(*test);
        if (!gate.has_value()) {
            return std::unexpected(gate.error());
        }
        Parsed<FloatProvider> onTrue = field("on_true");
        if (!onTrue.has_value()) {
            return onTrue;
        }
        Parsed<FloatProvider> onFalse = optionalField("on_false");
        if (!onFalse.has_value()) {
            return onFalse;
        }
        return FloatProvider::conditional(std::move(*gate), std::move(*onTrue), std::move(*onFalse));
    }
    if (type == "minecraft:number_dispatcher") {
        const std::vector<JsonValue>* cases = asArray(object.find("cases"));
        if (cases == nullptr) {
            return std::unexpected(missing("number_dispatcher cases"));
        }
        std::vector<std::pair<LootCondition, FloatProvider>> parsed;
        for (const JsonValue& each : *cases) {
            const JsonObject* one = each.get<JsonObject>();
            const JsonValue* test = one == nullptr ? nullptr : one->find("condition");
            const JsonValue* value = one == nullptr ? nullptr : one->find("value");
            if (test == nullptr || value == nullptr) {
                return std::unexpected(missing("number_dispatcher case"));
            }
            Parsed<LootCondition> gate = condition(*test);
            if (!gate.has_value()) {
                return std::unexpected(gate.error());
            }
            Parsed<FloatProvider> result = floatProvider(*value);
            if (!result.has_value()) {
                return result;
            }
            parsed.emplace_back(std::move(*gate), std::move(*result));
        }
        Parsed<FloatProvider> fallback = optionalField("default");
        if (!fallback.has_value()) {
            return fallback;
        }
        return FloatProvider::dispatcher(std::move(parsed), std::move(*fallback));
    }
    if (type == "minecraft:weighted_list") {
        const std::vector<JsonValue>* distribution = asArray(object.find("distribution"));
        if (distribution == nullptr || distribution->empty()) {
            return std::unexpected(missing("weighted_list distribution"));
        }
        std::vector<std::pair<FloatProvider, std::int32_t>> parsed;
        for (const JsonValue& each : *distribution) {
            const JsonObject* one = each.get<JsonObject>();
            const JsonValue* data = one == nullptr ? nullptr : one->find("data");
            const std::optional<double> weight = one == nullptr ? std::nullopt : asNumber(one->find("weight"));
            if (data == nullptr || !weight.has_value()) {
                return std::unexpected(missing("weighted_list entry"));
            }
            Parsed<FloatProvider> value = floatProvider(*data);
            if (!value.has_value()) {
                return value;
            }
            parsed.emplace_back(std::move(*value), static_cast<std::int32_t>(*weight));
        }
        return FloatProvider::weightedList(std::move(parsed));
    }
    if (type == "minecraft:enchantment_level") {
        return FloatProvider::enchantmentLevel(parseLevelBased(object.find("amount")));
    }
    if (type == "minecraft:storage") {
        const std::optional<wire::common::Identifier> storage = identifierAt(object, "storage");
        const std::optional<std::string> path = asString(object.find("path"));
        if (!storage.has_value() || !path.has_value()) {
            return std::unexpected(missing("storage access"));
        }
        Parsed<FloatProvider> fallback = optionalField("fallback");
        return fallback.has_value()
                   ? Parsed<FloatProvider>{FloatProvider::storage(*storage, *path, std::move(*fallback))}
                   : fallback;
    }
    if (type == "minecraft:environment_attribute") {
        const std::optional<wire::common::Identifier> attribute = identifierAt(object, "attribute");
        return attribute.has_value() ? Parsed<FloatProvider>{FloatProvider::environmentAttribute(*attribute)}
                                     : std::unexpected(missing("environment attribute"));
    }
    return std::unexpected(LootTableParseError{"Unsupported number provider type: " + type});
}

/// parity: `IntRangePredicate.CODEC`, a provider to equal tried before a `{min, max}` line.
Parsed<IntRange> Reader::intRange(const JsonValue& value) {
    const JsonObject* object = value.get<JsonObject>();
    if (object == nullptr || object->find("type") != nullptr) {
        Parsed<IntProvider> exact = intProvider(value);
        return exact.has_value() ? Parsed<IntRange>{IntRange{.exact = std::move(*exact)}}
                                 : std::unexpected(exact.error());
    }
    IntRange range;
    for (const std::string_view key : {std::string_view{"min"}, std::string_view{"max"}}) {
        if (const JsonValue* bound = object->find(key); bound != nullptr) {
            Parsed<IntProvider> parsed = intProvider(*bound);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            (key == "min" ? range.min : range.max) = std::move(*parsed);
        }
    }
    return range;
}

Parsed<FloatRange> Reader::floatRange(const JsonValue& value) {
    const JsonObject* object = value.get<JsonObject>();
    if (object == nullptr || object->find("type") != nullptr) {
        Parsed<FloatProvider> exact = floatProvider(value);
        return exact.has_value() ? Parsed<FloatRange>{FloatRange{.exact = std::move(*exact)}}
                                 : std::unexpected(exact.error());
    }
    FloatRange range;
    for (const std::string_view key : {std::string_view{"min"}, std::string_view{"max"}}) {
        if (const JsonValue* bound = object->find(key); bound != nullptr) {
            Parsed<FloatProvider> parsed = floatProvider(*bound);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            (key == "min" ? range.min : range.max) = std::move(*parsed);
        }
    }
    return range;
}

} // namespace basalt::server::world::loot::lootTableJson::parts
