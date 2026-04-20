#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/BlockStateJson.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/server/world/terrain/material/MaterialRules.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>
#include <set>
#include <utility>

namespace basalt::server::world::terrain::material {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] std::string withNamespace(std::string_view id) {
    return id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
}

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

[[nodiscard]] std::optional<bool> flag(const JsonObject& object, std::string_view field) {
    const JsonValue* value = object.find(field);
    const bool* found = value == nullptr ? nullptr : value->get<bool>();
    return found == nullptr ? std::nullopt : std::optional{*found};
}

[[nodiscard]] std::optional<VerticalAnchor> anchor(const JsonObject& object, std::string_view field) {
    const JsonValue* value = object.find(field);
    const auto* inner = value == nullptr ? nullptr : value->get<JsonObject>();
    if (inner == nullptr) {
        return std::nullopt;
    }
    if (const std::optional<double> absolute = number(*inner, "absolute"); absolute.has_value()) {
        return VerticalAnchor{VerticalAnchor::Kind::Absolute, static_cast<std::int32_t>(*absolute)};
    }
    if (const std::optional<double> bottom = number(*inner, "above_bottom"); bottom.has_value()) {
        return VerticalAnchor{VerticalAnchor::Kind::AboveBottom, static_cast<std::int32_t>(*bottom)};
    }
    if (const std::optional<double> top = number(*inner, "below_top"); top.has_value()) {
        return VerticalAnchor{VerticalAnchor::Kind::BelowTop, static_cast<std::int32_t>(*top)};
    }
    return std::nullopt;
}

/// parity: `MaterialRuleContext.LazyYCondition` and `LazyXZCondition`.
[[nodiscard]] ConditionEvaluator lazy(MaterialRuleContext& context, bool perColumn, std::function<bool()> compute) {
    struct State {
        std::int64_t stamp;
        bool value = false;
    };

    auto state =
        std::make_shared<State>(State{(perColumn ? context.updateXZStamp() : context.updateYStamp()) - 1, false});
    return [&context, perColumn, state, compute = std::move(compute)]() {
        const std::int64_t stamp = perColumn ? context.updateXZStamp() : context.updateYStamp();
        if (state->stamp != stamp) {
            state->stamp = stamp;
            state->value = compute();
        }
        return state->value;
    };
}

class BiomeCondition final : public MaterialCondition {
public:
    explicit BiomeCondition(std::set<std::string, std::less<>> biomes) : biomes_(std::move(biomes)) {}

    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return lazy(context, false, [this, &context]() { return biomes_.contains(context.biome()); });
    }

private:
    std::set<std::string, std::less<>> biomes_;
};

class NoiseThresholdCondition final : public MaterialCondition {
public:
    NoiseThresholdCondition(std::string noise, double min, double max, bool is3d)
        : noise_(std::move(noise)), min_(min), max_(max), is3d_(is3d) {}

    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        std::function<double()> sample = context.noiseSampler(noise_, is3d_);
        return [sample = std::move(sample), min = min_, max = max_]() {
            const double value = sample();
            return value >= min && value <= max;
        };
    }

private:
    std::string noise_;
    double min_;
    double max_;
    bool is3d_;
};

class VerticalGradientCondition final : public MaterialCondition {
public:
    VerticalGradientCondition(std::string name, VerticalAnchor trueAtAndBelow, VerticalAnchor falseAtAndAbove)
        : name_(std::move(name)), trueAtAndBelow_(trueAtAndBelow), falseAtAndAbove_(falseAtAndAbove) {}

    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        const std::int32_t trueAt = context.resolve(trueAtAndBelow_);
        const std::int32_t falseAt = context.resolve(falseAtAndAbove_);
        const random::PositionalRandomFactory factory = context.randomFactory(name_);
        return lazy(context, false, [&context, trueAt, falseAt, factory]() {
            const std::int32_t y = context.blockY();
            if (y <= trueAt) {
                return true;
            }
            if (y >= falseAt) {
                return false;
            }
            // parity: `Mth.map` in double.
            const double probability =
                1.0 + (static_cast<double>(y) - trueAt) / (static_cast<double>(falseAt) - trueAt) * (0.0 - 1.0);
            random::RandomSource random = factory.at(context.blockX(), y, context.blockZ());
            return static_cast<double>(random.nextFloat()) < probability;
        });
    }

private:
    std::string name_;
    VerticalAnchor trueAtAndBelow_;
    VerticalAnchor falseAtAndAbove_;
};

class YCondition final : public MaterialCondition {
public:
    YCondition(VerticalAnchor anchor, std::int32_t multiplier, bool addStoneDepth)
        : anchor_(anchor), multiplier_(multiplier), addStoneDepth_(addStoneDepth) {}

    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return lazy(context, false, [this, &context]() {
            return context.blockY() + (addStoneDepth_ ? context.stoneDepthAbove() : 0) >=
                   context.resolve(anchor_) + context.surfaceDepth() * multiplier_;
        });
    }

private:
    VerticalAnchor anchor_;
    std::int32_t multiplier_;
    bool addStoneDepth_;
};

class WaterCondition final : public MaterialCondition {
public:
    WaterCondition(std::int32_t offset, std::int32_t multiplier, bool addStoneDepth)
        : offset_(offset), multiplier_(multiplier), addStoneDepth_(addStoneDepth) {}

    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return lazy(context, false, [this, &context]() {
            return context.waterHeight() == std::numeric_limits<std::int32_t>::min() ||
                   context.blockY() + (addStoneDepth_ ? context.stoneDepthAbove() : 0) >=
                       context.waterHeight() + offset_ + context.surfaceDepth() * multiplier_;
        });
    }

private:
    std::int32_t offset_;
    std::int32_t multiplier_;
    bool addStoneDepth_;
};

class TemperatureCondition final : public MaterialCondition {
public:
    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return [&context]() {
            return biomeTemperature::coldEnoughToSnow(context.biome(), context.blockX(), context.blockY(),
                                                      context.blockZ());
        };
    }
};

class SteepCondition final : public MaterialCondition {
public:
    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return lazy(context, true, [&context]() { return context.gradientX() <= -4 || context.gradientZ() >= 4; });
    }
};

class HoleCondition final : public MaterialCondition {
public:
    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return lazy(context, true, [&context]() { return context.surfaceDepth() <= 0; });
    }
};

class AbovePreliminarySurfaceCondition final : public MaterialCondition {
public:
    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return [&context]() { return context.blockY() >= context.minSurfaceLevel(); };
    }
};

class NotCondition final : public MaterialCondition {
public:
    explicit NotCondition(std::shared_ptr<const MaterialCondition> target) : target_(std::move(target)) {}

    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        ConditionEvaluator target = target_->compile(context);
        return [target = std::move(target)]() { return !target(); };
    }

private:
    std::shared_ptr<const MaterialCondition> target_;
};

class StoneDepthCondition final : public MaterialCondition {
public:
    StoneDepthCondition(std::int32_t offset, bool addSurfaceDepth, std::int32_t secondaryRange, bool ceiling)
        : offset_(offset), addSurfaceDepth_(addSurfaceDepth), secondaryRange_(secondaryRange), ceiling_(ceiling) {}

    [[nodiscard]] ConditionEvaluator compile(MaterialRuleContext& context) const override {
        return lazy(context, false, [this, &context]() {
            const std::int32_t stoneDepth = ceiling_ ? context.stoneDepthBelow() : context.stoneDepthAbove();
            const std::int32_t surfaceDepth = addSurfaceDepth_ ? context.surfaceDepth() : 0;
            std::int32_t secondary = 0;
            if (secondaryRange_ != 0) {
                // parity: `(int) Mth.map(secondary, -1, 1, 0, range)`.
                const double mapped =
                    0.0 + (context.surfaceSecondary() - -1.0) / (1.0 - -1.0) * static_cast<double>(secondaryRange_);
                secondary = static_cast<std::int32_t>(mapped);
            }
            return stoneDepth <= 1 + offset_ + surfaceDepth + secondary;
        });
    }

private:
    std::int32_t offset_;
    bool addSurfaceDepth_;
    std::int32_t secondaryRange_;
    bool ceiling_;
};

class BlockRule final : public MaterialRule {
public:
    explicit BlockRule(std::int32_t state) : state_(state) {}

    [[nodiscard]] RuleEvaluator compile(MaterialRuleContext&) const override {
        return [state = state_](std::int32_t, std::int32_t, std::int32_t) { return std::optional{state}; };
    }

private:
    std::int32_t state_;
};

class BandlandsRule final : public MaterialRule {
public:
    [[nodiscard]] RuleEvaluator compile(MaterialRuleContext& context) const override {
        return
            [&context](std::int32_t x, std::int32_t y, std::int32_t z) { return std::optional{context.band(x, y, z)}; };
    }
};

class SequenceRule final : public MaterialRule {
public:
    explicit SequenceRule(std::vector<std::shared_ptr<const MaterialRule>> rules) : rules_(std::move(rules)) {}

    [[nodiscard]] RuleEvaluator compile(MaterialRuleContext& context) const override {
        if (rules_.size() == 1) {
            return rules_.front()->compile(context);
        }
        std::vector<RuleEvaluator> compiled;
        compiled.reserve(rules_.size());
        for (const auto& rule : rules_) {
            compiled.push_back(rule->compile(context));
        }
        return [compiled = std::move(compiled)](std::int32_t x, std::int32_t y,
                                                std::int32_t z) -> std::optional<std::int32_t> {
            for (const RuleEvaluator& rule : compiled) {
                if (std::optional<std::int32_t> state = rule(x, y, z); state.has_value()) {
                    return state;
                }
            }
            return std::nullopt;
        };
    }

private:
    std::vector<std::shared_ptr<const MaterialRule>> rules_;
};

class ConditionRule final : public MaterialRule {
public:
    ConditionRule(std::shared_ptr<const MaterialCondition> condition, std::shared_ptr<const MaterialRule> rule)
        : condition_(std::move(condition)), rule_(std::move(rule)) {}

    [[nodiscard]] RuleEvaluator compile(MaterialRuleContext& context) const override {
        ConditionEvaluator condition = condition_->compile(context);
        RuleEvaluator rule = rule_->compile(context);
        return [condition = std::move(condition),
                rule = std::move(rule)](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
            return condition() ? rule(x, y, z) : std::nullopt;
        };
    }

private:
    std::shared_ptr<const MaterialCondition> condition_;
    std::shared_ptr<const MaterialRule> rule_;
};

/// parity: `OreVeinRule`.
class OreVeinRule final : public MaterialRule {
public:
    OreVeinRule(std::int32_t ore, std::int32_t rawOre, std::int32_t filler, float rawOreChance,
                densityfunction::Function density, densityfunction::Function richness, densityfunction::Function gap)
        : ore_(ore), rawOre_(rawOre), filler_(filler), rawOreChance_(rawOreChance), density_(std::move(density)),
          richness_(std::move(richness)), gap_(std::move(gap)) {}

    [[nodiscard]] RuleEvaluator compile(MaterialRuleContext& context) const override {
        std::function<float()> density = context.densities(density_, true);
        std::function<float()> richness = context.densities(richness_, true);
        std::function<float()> gap = context.densities(gap_, false);
        const random::PositionalRandomFactory random = context.randomFactory("minecraft:ore");
        return [this, density = std::move(density), richness = std::move(richness), gap = std::move(gap),
                random](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
            const float veinDensity = density();
            if (veinDensity <= 0.0F) {
                return std::nullopt;
            }
            random::RandomSource source = random.at(x, y, z);
            if (source.nextFloat() > veinDensity) {
                return std::nullopt;
            }
            const float veinRichness = richness();
            if (source.nextFloat() < veinRichness && gap() < 0.0F) {
                return source.nextFloat() < rawOreChance_ ? rawOre_ : ore_;
            }
            return filler_;
        };
    }

private:
    std::int32_t ore_;
    std::int32_t rawOre_;
    std::int32_t filler_;
    float rawOreChance_;
    densityfunction::Function density_;
    densityfunction::Function richness_;
    densityfunction::Function gap_;
};

[[nodiscard]] std::expected<std::int32_t, std::string> stateField(const JsonObject& object, std::string_view field) {
    const JsonValue* value = object.find(field);
    const std::optional<std::int32_t> state = value == nullptr ? std::nullopt : blockStateJson::parse(*value);
    if (!state.has_value()) {
        return std::unexpected("invalid block state '" + std::string{field} + "'");
    }
    return *state;
}

[[nodiscard]] std::optional<std::string> typeOf(const JsonObject& object) {
    const JsonValue* value = object.find("type");
    const auto* text = value == nullptr ? nullptr : value->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    std::string_view type{*text};
    if (type.starts_with("minecraft:")) {
        type.remove_prefix(10);
    }
    return std::string{type};
}

[[nodiscard]] std::expected<JsonValue, std::string> readJson(densityfunction::DensityFunctionLoader& loader,
                                                             std::string_view registry, const std::string& id) {
    const std::optional<std::string> text = loader.reader()(densityfunction::resourcePath(registry, id));
    if (!text.has_value()) {
        return std::unexpected(std::string{registry} + " '" + id + "' is missing");
    }
    std::expected<JsonValue, wire::common::JsonError> json = wire::common::parseJson(*text);
    if (!json.has_value()) {
        return std::unexpected(std::string{registry} + " '" + id + "' is not valid JSON");
    }
    return std::move(*json);
}

} // namespace

MaterialRuleLoader::MaterialRuleLoader(densityfunction::DensityFunctionLoader& functions) : functions_(&functions) {}

std::expected<std::shared_ptr<const MaterialCondition>, std::string>
MaterialRuleLoader::condition(const JsonValue& json) {
    if (const auto* id = json.get<std::string>(); id != nullptr) {
        const std::string key = withNamespace(*id);
        if (const auto found = conditions_.find(key); found != conditions_.end()) {
            return found->second;
        }
        std::expected<JsonValue, std::string> file = readJson(*functions_, "worldgen/material_condition", key);
        if (!file.has_value()) {
            return std::unexpected(file.error());
        }
        auto parsed = condition(*file);
        if (!parsed.has_value()) {
            return std::unexpected("material condition '" + key + "': " + parsed.error());
        }
        conditions_.emplace(key, *parsed);
        return parsed;
    }
    const auto* object = json.get<JsonObject>();
    const std::optional<std::string> type = object == nullptr ? std::nullopt : typeOf(*object);
    if (!type.has_value()) {
        return std::unexpected(std::string{"a material condition needs a type"});
    }
    if (*type == "biome") {
        std::set<std::string, std::less<>> biomes;
        const JsonValue* field = object->find("biome_is");
        if (const auto* single = field == nullptr ? nullptr : field->get<std::string>(); single != nullptr) {
            if (single->starts_with("#")) {
                return std::unexpected(std::string{"biome: tags are not supported here"});
            }
            biomes.insert(withNamespace(*single));
        } else if (const auto* list = field == nullptr ? nullptr : field->get<std::vector<JsonValue>>();
                   list != nullptr) {
            for (const JsonValue& entry : *list) {
                if (const auto* name = entry.get<std::string>(); name != nullptr) {
                    biomes.insert(withNamespace(*name));
                }
            }
        } else {
            return std::unexpected(std::string{"biome: missing 'biome_is'"});
        }
        return std::make_shared<const BiomeCondition>(std::move(biomes));
    }
    if (*type == "noise_threshold") {
        const JsonValue* noise = object->find("noise");
        const auto* id = noise == nullptr ? nullptr : noise->get<std::string>();
        const std::optional<double> min = number(*object, "min_threshold");
        const std::optional<double> max = number(*object, "max_threshold");
        if (id == nullptr || !min.has_value() || !max.has_value()) {
            return std::unexpected(std::string{"noise_threshold: needs noise and thresholds"});
        }
        return std::make_shared<const NoiseThresholdCondition>(withNamespace(*id), *min, *max,
                                                               flag(*object, "is_3d").value_or(false));
    }
    if (*type == "vertical_gradient") {
        const JsonValue* name = object->find("random_name");
        const auto* text = name == nullptr ? nullptr : name->get<std::string>();
        const std::optional<VerticalAnchor> trueAt = anchor(*object, "true_at_and_below");
        const std::optional<VerticalAnchor> falseAt = anchor(*object, "false_at_and_above");
        if (text == nullptr || !trueAt.has_value() || !falseAt.has_value()) {
            return std::unexpected(std::string{"vertical_gradient: needs random_name and anchors"});
        }
        return std::make_shared<const VerticalGradientCondition>(withNamespace(*text), *trueAt, *falseAt);
    }
    if (*type == "y_above") {
        const std::optional<VerticalAnchor> at = anchor(*object, "anchor");
        const std::optional<double> multiplier = number(*object, "surface_depth_multiplier");
        const std::optional<bool> add = flag(*object, "add_stone_depth");
        if (!at.has_value() || !multiplier.has_value() || !add.has_value()) {
            return std::unexpected(std::string{"y_above: incomplete"});
        }
        return std::make_shared<const YCondition>(*at, static_cast<std::int32_t>(*multiplier), *add);
    }
    if (*type == "water") {
        const std::optional<double> offset = number(*object, "offset");
        const std::optional<double> multiplier = number(*object, "surface_depth_multiplier");
        const std::optional<bool> add = flag(*object, "add_stone_depth");
        if (!offset.has_value() || !multiplier.has_value() || !add.has_value()) {
            return std::unexpected(std::string{"water: incomplete"});
        }
        return std::make_shared<const WaterCondition>(static_cast<std::int32_t>(*offset),
                                                      static_cast<std::int32_t>(*multiplier), *add);
    }
    if (*type == "temperature") {
        return std::make_shared<const TemperatureCondition>();
    }
    if (*type == "steep") {
        return std::make_shared<const SteepCondition>();
    }
    if (*type == "hole") {
        return std::make_shared<const HoleCondition>();
    }
    if (*type == "above_preliminary_surface") {
        return std::make_shared<const AbovePreliminarySurfaceCondition>();
    }
    if (*type == "not") {
        const JsonValue* invert = object->find("invert");
        if (invert == nullptr) {
            return std::unexpected(std::string{"not: missing 'invert'"});
        }
        auto target = condition(*invert);
        if (!target.has_value()) {
            return std::unexpected(target.error());
        }
        return std::make_shared<const NotCondition>(*target);
    }
    if (*type == "stone_depth") {
        const std::optional<double> offset = number(*object, "offset");
        const std::optional<bool> add = flag(*object, "add_surface_depth");
        const std::optional<double> range = number(*object, "secondary_depth_range");
        const JsonValue* surface = object->find("surface_type");
        const auto* surfaceType = surface == nullptr ? nullptr : surface->get<std::string>();
        if (!offset.has_value() || !add.has_value() || !range.has_value() || surfaceType == nullptr) {
            return std::unexpected(std::string{"stone_depth: incomplete"});
        }
        return std::make_shared<const StoneDepthCondition>(
            static_cast<std::int32_t>(*offset), *add, static_cast<std::int32_t>(*range), *surfaceType == "ceiling");
    }
    return std::unexpected("unsupported material condition '" + *type + "'");
}

std::expected<std::shared_ptr<const MaterialRule>, std::string> MaterialRuleLoader::rule(const JsonValue& json) {
    if (const auto* id = json.get<std::string>(); id != nullptr) {
        const std::string key = withNamespace(*id);
        if (const auto found = rules_.find(key); found != rules_.end()) {
            return found->second;
        }
        std::expected<JsonValue, std::string> file = readJson(*functions_, "worldgen/material_rule", key);
        if (!file.has_value()) {
            return std::unexpected(file.error());
        }
        auto parsed = rule(*file);
        if (!parsed.has_value()) {
            return std::unexpected("material rule '" + key + "': " + parsed.error());
        }
        rules_.emplace(key, *parsed);
        return parsed;
    }
    const auto* object = json.get<JsonObject>();
    const std::optional<std::string> type = object == nullptr ? std::nullopt : typeOf(*object);
    if (!type.has_value()) {
        return std::unexpected(std::string{"a material rule needs a type"});
    }
    if (*type == "block") {
        std::expected<std::int32_t, std::string> state = stateField(*object, "result_state");
        if (!state.has_value()) {
            return std::unexpected(state.error());
        }
        return std::make_shared<const BlockRule>(*state);
    }
    if (*type == "bandlands") {
        return std::make_shared<const BandlandsRule>();
    }
    if (*type == "sequence") {
        const JsonValue* field = object->find("sequence");
        const auto* list = field == nullptr ? nullptr : field->get<std::vector<JsonValue>>();
        if (list == nullptr || list->empty()) {
            return std::unexpected(std::string{"sequence: needs at least one rule"});
        }
        std::vector<std::shared_ptr<const MaterialRule>> rules;
        for (const JsonValue& entry : *list) {
            auto parsed = rule(entry);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            rules.push_back(*parsed);
        }
        return std::make_shared<const SequenceRule>(std::move(rules));
    }
    if (*type == "condition") {
        const JsonValue* ifTrue = object->find("if_true");
        const JsonValue* thenRun = object->find("then_run");
        if (ifTrue == nullptr || thenRun == nullptr) {
            return std::unexpected(std::string{"condition: needs if_true and then_run"});
        }
        auto parsedCondition = condition(*ifTrue);
        if (!parsedCondition.has_value()) {
            return std::unexpected(parsedCondition.error());
        }
        auto parsedRule = rule(*thenRun);
        if (!parsedRule.has_value()) {
            return std::unexpected(parsedRule.error());
        }
        return std::make_shared<const ConditionRule>(*parsedCondition, *parsedRule);
    }
    if (*type == "ore_vein") {
        auto ore = stateField(*object, "ore_block");
        auto rawOre = stateField(*object, "raw_ore_block");
        auto filler = stateField(*object, "filler_block");
        const std::optional<double> chance = number(*object, "raw_ore_chance");
        if (!ore.has_value() || !rawOre.has_value() || !filler.has_value() || !chance.has_value()) {
            return std::unexpected(std::string{"ore_vein: incomplete"});
        }
        const auto function =
            [this, object](std::string_view field) -> std::expected<densityfunction::Function, std::string> {
            const JsonValue* value = object->find(field);
            if (value == nullptr) {
                return std::unexpected("ore_vein: missing '" + std::string{field} + "'");
            }
            return functions_->parse(*value);
        };
        auto density = function("density");
        auto richness = function("richness");
        auto gap = function("filler_gap");
        if (!density.has_value() || !richness.has_value() || !gap.has_value()) {
            return std::unexpected(!density.has_value() ? density.error()
                                                        : (!richness.has_value() ? richness.error() : gap.error()));
        }
        return std::make_shared<const OreVeinRule>(*ore, *rawOre, *filler, static_cast<float>(*chance), *density,
                                                   *richness, *gap);
    }
    return std::unexpected("unsupported material rule '" + *type + "'");
}

} // namespace basalt::server::world::terrain::material
