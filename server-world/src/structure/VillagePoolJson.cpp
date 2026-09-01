#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/VillagePoolJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <map>
#include <utility>
#include <vector>

namespace basalt::server::world::structure {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] const JsonObject* objectAt(const JsonObject& parent, std::string_view key) {
    const JsonValue* found = parent.find(key);
    return found == nullptr ? nullptr : found->get<JsonObject>();
}

[[nodiscard]] const std::vector<JsonValue>* arrayAt(const JsonObject& parent, std::string_view key) {
    const JsonValue* found = parent.find(key);
    return found == nullptr ? nullptr : found->get<std::vector<JsonValue>>();
}

[[nodiscard]] std::optional<std::string> stringAt(const JsonObject& parent, std::string_view key) {
    const JsonValue* found = parent.find(key);
    if (found == nullptr) {
        return std::nullopt;
    }
    if (const std::string* text = found->get<std::string>(); text != nullptr) {
        return *text;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::int32_t> intAt(const JsonObject& parent, std::string_view key) {
    const JsonValue* found = parent.find(key);
    if (found == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* number = found->get<std::int64_t>(); number != nullptr) {
        return static_cast<std::int32_t>(*number);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<float> floatAt(const JsonObject& parent, std::string_view key) {
    const JsonValue* found = parent.find(key);
    if (found == nullptr) {
        return std::nullopt;
    }
    if (const double* number = found->get<double>(); number != nullptr) {
        return static_cast<float>(*number);
    }
    if (const std::int64_t* number = found->get<std::int64_t>(); number != nullptr) {
        return static_cast<float>(*number);
    }
    return std::nullopt;
}

/// A block's own default state, which is what a name with no properties means.
[[nodiscard]] std::int32_t defaultStateId(const wire::common::Identifier& block) {
    const registry::BlockState* found = registry::blocks::named(block.path());
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::int32_t blockStateIdOf(const JsonObject& json) {
    if (const std::optional<std::string> name = stringAt(json, "Name"); name.has_value()) {
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> block =
            wire::common::Identifier::parse(*name);
        if (!block.has_value()) {
            return registry::blocks::air().stateId;
        }
        std::map<std::string, std::string> properties;
        if (const JsonObject* raw = objectAt(json, "Properties"); raw != nullptr) {
            for (std::size_t index = 0; index < raw->size(); ++index) {
                if (const std::string* value = raw->values()[index].get<std::string>(); value != nullptr) {
                    properties.emplace(raw->names()[index], *value);
                }
            }
        }
        if (const std::optional<std::int32_t> exact = registry::blocks::stateIdOf(*block, properties);
            exact.has_value()) {
            return *exact;
        }
        return defaultStateId(*block);
    }
    const std::optional<std::string> blockName = stringAt(json, "block");
    if (!blockName.has_value()) {
        return registry::blocks::air().stateId;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> block =
        wire::common::Identifier::parse(*blockName);
    if (!block.has_value()) {
        return registry::blocks::air().stateId;
    }
    return defaultStateId(*block);
}

[[nodiscard]] std::optional<StructureRuleTest> parseRuleTest(const JsonObject* json,
                                                             const villagePoolJson::TagResolver& resolveTag) {
    if (json == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::string> type = stringAt(*json, "predicate_type");
    if (!type.has_value()) {
        return std::nullopt;
    }
    if (*type == "minecraft:random_block_match") {
        return StructureRuleTest::randomBlockMatch(blockStateIdOf(*json), floatAt(*json, "probability").value_or(1.0F));
    }
    if (*type == "minecraft:block_match") {
        return StructureRuleTest::blockMatch(blockStateIdOf(*json));
    }
    if (*type == "minecraft:blockstate_match") {
        const JsonObject* state = objectAt(*json, "block_state");
        if (state == nullptr) {
            state = objectAt(*json, "state");
        }
        return StructureRuleTest::blockStateMatch(state == nullptr ? registry::blocks::air().stateId
                                                                   : blockStateIdOf(*state));
    }
    if (*type == "minecraft:tag_match") {
        const std::optional<std::string> tag = stringAt(*json, "tag");
        if (!tag.has_value()) {
            return std::nullopt;
        }
        const std::optional<std::set<std::int32_t>> members = resolveTag ? resolveTag(*tag) : std::nullopt;
        return StructureRuleTest::tagMatch(members.value_or(std::set<std::int32_t>{}));
    }
    return std::nullopt;
}

[[nodiscard]] StructureProcessorRule parseRule(const JsonObject& json, const villagePoolJson::TagResolver& resolveTag) {
    std::optional<std::string> loot;
    if (const JsonObject* modifier = objectAt(json, "block_entity_modifier"); modifier != nullptr) {
        if (stringAt(*modifier, "type") == "minecraft:append_loot") {
            loot = stringAt(*modifier, "loot_table");
        }
    }
    const JsonObject* output = objectAt(json, "output_state");
    return StructureProcessorRule{
        .inputTest = parseRuleTest(objectAt(json, "input_predicate"), resolveTag)
                         .value_or(StructureRuleTest::blockMatch(registry::blocks::air().stateId)),
        .locationTest = parseRuleTest(objectAt(json, "location_predicate"), resolveTag),
        .outputStateId = output == nullptr ? registry::blocks::air().stateId : blockStateIdOf(*output),
        .appendLootTable = std::move(loot)};
}

[[nodiscard]] StructureProcessorList parseProcessorListObject(const JsonObject& root,
                                                              const villagePoolJson::TagResolver& resolveTag) {
    const std::vector<JsonValue>* processors = arrayAt(root, "processors");
    if (processors == nullptr) {
        return StructureProcessorList{};
    }
    std::vector<StructureProcessorRule> rules;
    std::optional<BlockRot> rot;
    std::optional<std::set<std::int32_t>> protectedBlocks;
    std::vector<CappedRules> capped;
    for (const JsonValue& entry : *processors) {
        const JsonObject* processor = entry.get<JsonObject>();
        if (processor == nullptr) {
            continue;
        }
        const std::optional<std::string> type = stringAt(*processor, "processor_type");
        if (!type.has_value()) {
            continue;
        }
        if (*type == "minecraft:rule") {
            if (const std::vector<JsonValue>* entries = arrayAt(*processor, "rules"); entries != nullptr) {
                for (const JsonValue& rule : *entries) {
                    if (const JsonObject* object = rule.get<JsonObject>(); object != nullptr) {
                        rules.push_back(parseRule(*object, resolveTag));
                    }
                }
            }
        } else if (*type == "minecraft:block_rot") {
            std::optional<std::set<std::int32_t>> rottable;
            if (const std::optional<std::string> tag = stringAt(*processor, "rottable_blocks");
                tag.has_value() && resolveTag) {
                rottable = resolveTag(*tag);
            }
            rot =
                BlockRot{.integrity = floatAt(*processor, "integrity").value_or(1.0F), .rottable = std::move(rottable)};
        } else if (*type == "minecraft:capped") {
            std::vector<StructureProcessorRule> delegate;
            if (const JsonObject* inner = objectAt(*processor, "delegate"); inner != nullptr) {
                if (const std::vector<JsonValue>* entries = arrayAt(*inner, "rules"); entries != nullptr) {
                    for (const JsonValue& rule : *entries) {
                        if (const JsonObject* object = rule.get<JsonObject>(); object != nullptr) {
                            delegate.push_back(parseRule(*object, resolveTag));
                        }
                    }
                }
            }
            // `limit` is an IntProvider in the codec; every list in 26.2 uses a constant.
            if (!delegate.empty()) {
                capped.push_back(
                    CappedRules{.delegate = std::move(delegate), .limit = intAt(*processor, "limit").value_or(0)});
            }
        } else if (*type == "minecraft:protected_blocks") {
            if (const std::optional<std::string> tag = stringAt(*processor, "value"); tag.has_value() && resolveTag) {
                const std::string_view name = std::string_view{*tag}.substr(tag->starts_with('#') ? 1 : 0);
                protectedBlocks = resolveTag(name);
            }
        }
        // Everything else is either a no-op here or belongs to a structure that does not read this
        // yet; a processor list is data, so an unknown one is skipped, not fatal.
    }
    return StructureProcessorList{std::move(rules), std::move(rot), std::move(protectedBlocks), std::move(capped)};
}

[[nodiscard]] StructureProjection parseProjection(const JsonObject& json) {
    return stringAt(json, "projection") == "terrain_matching" ? StructureProjection::TerrainMatching
                                                              : StructureProjection::Rigid;
}

[[nodiscard]] std::expected<StructurePoolElement, villagePoolJson::ParseError>
parseElement(const JsonObject& json, const villagePoolJson::ProcessorListResolver& resolveProcessors,
             const villagePoolJson::TagResolver& resolveTag);

[[nodiscard]] StructureProcessorList
parseProcessorsField(const JsonValue* field, const villagePoolJson::ProcessorListResolver& resolveProcessors,
                     const villagePoolJson::TagResolver& resolveTag) {
    if (field == nullptr) {
        return StructureProcessorList{};
    }
    if (const std::string* name = field->get<std::string>(); name != nullptr) {
        const std::optional<std::string> text = resolveProcessors ? resolveProcessors(*name) : std::nullopt;
        if (!text.has_value()) {
            return StructureProcessorList{};
        }
        const std::expected<wire::common::JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(*text);
        if (!parsed.has_value()) {
            return StructureProcessorList{};
        }
        const JsonObject* root = parsed->get<JsonObject>();
        return root == nullptr ? StructureProcessorList{} : parseProcessorListObject(*root, resolveTag);
    }
    if (const JsonObject* object = field->get<JsonObject>(); object != nullptr) {
        return parseProcessorListObject(*object, resolveTag);
    }
    return StructureProcessorList{};
}

std::expected<StructurePoolElement, villagePoolJson::ParseError>
parseElement(const JsonObject& json, const villagePoolJson::ProcessorListResolver& resolveProcessors,
             const villagePoolJson::TagResolver& resolveTag) {
    const std::optional<std::string> type = stringAt(json, "element_type");
    if (!type.has_value()) {
        return std::unexpected(villagePoolJson::ParseError{"missing 'element_type'"});
    }
    if (*type == "minecraft:empty_pool_element") {
        return StructurePoolElement::empty();
    }
    if (*type == "minecraft:feature_pool_element") {
        const std::optional<std::string> feature = stringAt(json, "feature");
        if (!feature.has_value()) {
            return std::unexpected(villagePoolJson::ParseError{"missing 'feature'"});
        }
        return StructurePoolElement::feature(*feature, parseProjection(json));
    }
    if (*type == "minecraft:legacy_single_pool_element" || *type == "minecraft:single_pool_element") {
        const std::optional<std::string> location = stringAt(json, "location");
        if (!location.has_value()) {
            return std::unexpected(villagePoolJson::ParseError{"missing 'location'"});
        }
        return StructurePoolElement::legacySingle(
            *location, parseProcessorsField(json.find("processors"), resolveProcessors, resolveTag),
            parseProjection(json), *type == "minecraft:legacy_single_pool_element");
    }
    if (*type == "minecraft:list_pool_element") {
        std::vector<StructurePoolElement> elements;
        if (const std::vector<JsonValue>* entries = arrayAt(json, "elements"); entries != nullptr) {
            for (const JsonValue& entry : *entries) {
                const JsonObject* object = entry.get<JsonObject>();
                if (object == nullptr) {
                    return std::unexpected(villagePoolJson::ParseError{"list element is not an object"});
                }
                std::expected<StructurePoolElement, villagePoolJson::ParseError> element =
                    parseElement(*object, resolveProcessors, resolveTag);
                if (!element.has_value()) {
                    return element;
                }
                elements.push_back(std::move(*element));
            }
        }
        return StructurePoolElement::list(std::move(elements), parseProjection(json));
    }
    return StructurePoolElement::empty();
}

} // namespace

std::expected<StructureTemplatePool, villagePoolJson::ParseError>
villagePoolJson::parsePool(std::string_view json, const ProcessorListResolver& resolveProcessors,
                           const TagResolver& resolveTag) {
    const std::expected<wire::common::JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::unexpected(ParseError{"malformed json"});
    }
    const JsonObject* root = parsed->get<JsonObject>();
    if (root == nullptr) {
        return std::unexpected(ParseError{"root is not an object"});
    }
    std::vector<std::pair<StructurePoolElement, std::int32_t>> weighted;
    if (const std::vector<JsonValue>* elements = arrayAt(*root, "elements"); elements != nullptr) {
        for (const JsonValue& entry : *elements) {
            const JsonObject* object = entry.get<JsonObject>();
            if (object == nullptr) {
                return std::unexpected(ParseError{"pool entry is not an object"});
            }
            const JsonObject* element = objectAt(*object, "element");
            if (element == nullptr) {
                return std::unexpected(ParseError{"missing 'element'"});
            }
            std::expected<StructurePoolElement, ParseError> parsedElement =
                parseElement(*element, resolveProcessors, resolveTag);
            if (!parsedElement.has_value()) {
                return std::unexpected(parsedElement.error());
            }
            weighted.emplace_back(std::move(*parsedElement), intAt(*object, "weight").value_or(1));
        }
    }
    return StructureTemplatePool{stringAt(*root, "fallback").value_or("minecraft:empty"), weighted};
}

std::expected<StructureProcessorList, villagePoolJson::ParseError>
villagePoolJson::parseProcessorList(std::string_view json, const TagResolver& resolveTag) {
    const std::expected<wire::common::JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::unexpected(ParseError{"malformed json"});
    }
    const JsonObject* root = parsed->get<JsonObject>();
    if (root == nullptr) {
        return std::unexpected(ParseError{"root is not an object"});
    }
    return parseProcessorListObject(*root, resolveTag);
}

} // namespace basalt::server::world::structure
