#include "LootComponentEdit.hpp"
#include "LootJsonParts.hpp"

#include <basalt/wire/nbt/JsonToNbt.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <utility>

namespace basalt::server::world::loot::lootTableJson::parts {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using Op = ComponentEdit::Op;

constexpr std::array<std::string_view, 5> kShapes{"small_ball", "large_ball", "star", "creeper", "burst"};
constexpr std::array<std::string_view, 3> kOperations{"add_value", "add_multiplied_base", "add_multiplied_total"};
constexpr std::array<std::string_view, 11> kSlotGroups{"any",   "mainhand", "offhand", "hand", "feet",  "legs",
                                                       "chest", "head",     "armor",   "body", "saddle"};
constexpr std::array<std::string_view, 3> kContainers{"minecraft:container", "minecraft:bundle_contents",
                                                      "minecraft:charged_projectiles"};
/// parity: `ExtraCodecs.UNSIGNED_BYTE`.
constexpr std::int32_t kMaxUnsignedByte = 255;
/// parity: `WrittenBookContent.MAX_GENERATION`.
constexpr std::int32_t kMaxGeneration = 3;

[[nodiscard]] LootTableParseError invalid(std::string_view what) {
    return LootTableParseError{"Invalid " + std::string{what}};
}

[[nodiscard]] std::optional<NbtTag> nbtOf(const JsonValue& value) {
    std::expected<NbtTag, wire::nbt::JsonToNbtError> tag = wire::nbt::jsonToNbt(value);
    return tag.has_value() ? std::optional{std::move(*tag)} : std::nullopt;
}

/// parity: the `DataComponentType` names in a codec.
[[nodiscard]] std::optional<std::string> componentName(std::string_view name) {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(name);
    return id.has_value() ? std::optional{id->toString()} : std::nullopt;
}

/// parity: `ListOperation.codec`: `mode` and the fields it owns, read off `object`.
[[nodiscard]] Parsed<ListOperation> listOperation(const JsonObject& object) {
    static constexpr std::array<std::pair<std::string_view, ListOperation::Mode>, 4> kModes{{
        {"replace_all", ListOperation::Mode::ReplaceAll},
        {"replace_section", ListOperation::Mode::ReplaceSection},
        {"insert", ListOperation::Mode::Insert},
        {"append", ListOperation::Mode::Append},
    }};
    const std::optional<std::string> mode = asString(object.find("mode"));
    const auto found = std::ranges::find_if(kModes, [&mode](const auto& each) { return mode == each.first; });
    if (found == kModes.end()) {
        return std::unexpected(missing("list operation mode"));
    }
    ListOperation operation{.mode = found->second};
    const std::optional<double> offset = asNumber(object.find("offset"));
    const std::optional<double> size = asNumber(object.find("size"));
    if ((offset.has_value() && *offset < 0) || (size.has_value() && *size < 0)) {
        return std::unexpected(invalid("list operation offset"));
    }
    if (operation.mode == ListOperation::Mode::ReplaceSection || operation.mode == ListOperation::Mode::Insert) {
        operation.offset = static_cast<std::int32_t>(offset.value_or(0.0));
    }
    if (operation.mode == ListOperation::Mode::ReplaceSection && size.has_value()) {
        operation.sectionSize = static_cast<std::int32_t>(*size);
    }
    return operation;
}

/// parity: `Filterable.codec`.
[[nodiscard]] Parsed<NbtTag> filterable(const JsonValue& value, bool text) {
    const JsonObject* object = value.get<JsonObject>();
    const JsonValue* raw = object != nullptr && object->find("raw") != nullptr ? object->find("raw") : &value;
    const JsonValue* filtered = raw == &value ? nullptr : object->find("filtered");
    NbtCompound full;
    for (const auto& [key, part] : {std::pair{"raw", raw}, std::pair{"filtered", filtered}}) {
        if (part == nullptr) {
            continue;
        }
        if (!text && part->get<std::string>() == nullptr) {
            return std::unexpected(invalid("page"));
        }
        std::optional<NbtTag> tag = nbtOf(*part);
        if (!tag.has_value()) {
            return std::unexpected(invalid("page"));
        }
        full.set(key, std::move(*tag));
    }
    return NbtTag::ofCompound(std::move(full));
}

/// parity: `FireworkExplosion.CODEC`, written back without its defaults.
[[nodiscard]] Parsed<NbtTag> explosion(const JsonValue& value) {
    const JsonObject* object = value.get<JsonObject>();
    const std::optional<std::string> shape = object == nullptr ? std::nullopt : asString(object->find("shape"));
    if (!shape.has_value() || std::ranges::find(kShapes, *shape) == kShapes.end()) {
        return std::unexpected(invalid("firework explosion shape"));
    }
    NbtCompound built;
    built.putString("shape", *shape);
    for (const std::string_view key : {std::string_view{"colors"}, std::string_view{"fade_colors"}}) {
        const std::vector<JsonValue>* colors = asArray(object->find(key));
        if (colors == nullptr || colors->empty()) {
            continue;
        }
        std::vector<std::int32_t> ints;
        for (const JsonValue& color : *colors) {
            const std::optional<double> number = asNumber(&color);
            if (!number.has_value()) {
                return std::unexpected(invalid("firework colour"));
            }
            ints.push_back(static_cast<std::int32_t>(*number));
        }
        built.putIntArray(key, std::move(ints));
    }
    for (const std::string_view key : {std::string_view{"has_trail"}, std::string_view{"has_twinkle"}}) {
        if (asBool(object->find(key)).value_or(false)) {
            built.putBoolean(key, true);
        }
    }
    return NbtTag::ofCompound(std::move(built));
}

/// parity: `ListOperation.StandAlone.codec`, `{values, mode...}`.
template <typename Element>
[[nodiscard]] Parsed<std::pair<std::vector<Element>, ListOperation>>
standAlone(const JsonObject& object, const std::function<Parsed<Element>(const JsonValue&)>& element) {
    const std::vector<JsonValue>* values = asArray(object.find("values"));
    if (values == nullptr) {
        return std::unexpected(missing("list operation values"));
    }
    std::pair<std::vector<Element>, ListOperation> built;
    for (const JsonValue& value : *values) {
        Parsed<Element> parsed = element(value);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        built.first.push_back(std::move(*parsed));
    }
    Parsed<ListOperation> operation = listOperation(object);
    if (!operation.has_value()) {
        return std::unexpected(operation.error());
    }
    built.second = *operation;
    return built;
}

/// parity: `ExtraCodecs.RGB_COLOR_CODEC`'s vector form.
[[nodiscard]] std::optional<std::int32_t> rgbOf(const JsonValue& value) {
    const std::vector<JsonValue>* channels = value.get<std::vector<JsonValue>>();
    if (channels == nullptr || channels->size() != 3) {
        return std::nullopt;
    }
    std::int32_t color = 0xFF;
    for (const JsonValue& channel : *channels) {
        const std::optional<double> number = asNumber(&channel);
        if (!number.has_value()) {
            return std::nullopt;
        }
        color = (color << 8) | (static_cast<std::int32_t>(std::floor(static_cast<float>(*number) * 255.0F)) & 0xFF);
    }
    return color;
}

[[nodiscard]] std::shared_ptr<const ComponentEdit> share(ComponentEdit edit) {
    return std::make_shared<const ComponentEdit>(std::move(edit));
}

} // namespace

std::optional<Parsed<LootFunction>> Reader::componentFunction(const std::string& type, const JsonObject& object) {
    static const std::map<std::string, Op, std::less<>> kOps{
        {"minecraft:toggle_tooltips", Op::ToggleTooltips},
        {"minecraft:set_custom_data", Op::SetCustomData},
        {"minecraft:set_custom_model_data", Op::SetCustomModelData},
        {"minecraft:set_lore", Op::SetLore},
        {"minecraft:set_book_cover", Op::SetBookCover},
        {"minecraft:set_written_book_pages", Op::SetWrittenBookPages},
        {"minecraft:set_writable_book_pages", Op::SetWritableBookPages},
        {"minecraft:set_firework_explosion", Op::SetFireworkExplosion},
        {"minecraft:set_fireworks", Op::SetFireworks},
        {"minecraft:set_banner_pattern", Op::SetBannerPattern},
        {"minecraft:set_item", Op::SetItem},
        {"minecraft:set_loot_table", Op::SetLootTable},
        {"minecraft:fill_player_head", Op::FillPlayerHead},
        {"minecraft:set_attributes", Op::SetAttributes},
        {"minecraft:set_contents", Op::SetContents},
        {"minecraft:modify_contents", Op::ModifyContents},
    };
    const auto found = kOps.find(type);
    if (found == kOps.end()) {
        return std::nullopt;
    }
    ComponentEdit edit{.op = found->second};
    const auto done = [&edit]() -> Parsed<LootFunction> {
        return LootFunction::editComponents(share(std::move(edit)));
    };
    switch (edit.op) {
    case Op::ToggleTooltips: {
        const JsonObject* toggles = asObject(object.find("toggles"));
        if (toggles == nullptr) {
            return std::unexpected(missing("toggle_tooltips toggles"));
        }
        for (std::size_t index = 0; index < toggles->names().size(); ++index) {
            const std::optional<std::string> name = componentName(toggles->names()[index]);
            const std::optional<bool> shown = asBool(&toggles->values()[index]);
            if (!name.has_value() || !shown.has_value()) {
                return std::unexpected(invalid("tooltip toggle"));
            }
            edit.toggles.emplace_back(*name, *shown);
        }
        return done();
    }
    // parity: `TagParser.LENIENT_CODEC`, a string of SNBT or the compound written out.
    case Op::SetCustomData: {
        const JsonValue* tag = object.find("tag");
        std::optional<NbtTag> parsed;
        if (const std::optional<std::string> text = asString(tag); text.has_value()) {
            std::expected<NbtTag, wire::nbt::NbtError> read = wire::nbt::snbt::parse(*text);
            parsed = read.has_value() ? std::optional{std::move(*read)} : std::nullopt;
        } else if (tag != nullptr && tag->get<JsonObject>() != nullptr) {
            parsed = nbtOf(*tag);
        }
        const NbtCompound* compound = parsed.has_value() ? parsed->get<NbtCompound>() : nullptr;
        if (compound == nullptr) {
            return std::unexpected(invalid("set_custom_data tag"));
        }
        edit.compound = *compound;
        return done();
    }
    case Op::SetCustomModelData: {
        if (const JsonObject* floats = asObject(object.find("floats")); floats != nullptr) {
            auto parsed = standAlone<FloatProvider>(*floats, [this](const JsonValue& v) { return floatProvider(v); });
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            edit.floats = std::move(*parsed);
        }
        for (const bool flags : {true, false}) {
            const JsonObject* list = asObject(object.find(flags ? "flags" : "strings"));
            if (list == nullptr) {
                continue;
            }
            auto parsed = standAlone<NbtTag>(*list, [flags](const JsonValue& v) -> Parsed<NbtTag> {
                if (flags && v.get<bool>() != nullptr) {
                    return NbtTag::ofBoolean(*v.get<bool>());
                }
                if (!flags && v.get<std::string>() != nullptr) {
                    return NbtTag::ofString(*v.get<std::string>());
                }
                return std::unexpected(invalid("custom model data value"));
            });
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            (flags ? edit.flags : edit.strings) = std::move(*parsed);
        }
        if (const JsonObject* colors = asObject(object.find("colors")); colors != nullptr) {
            auto parsed = standAlone<IntProvider>(*colors, [this](const JsonValue& v) -> Parsed<IntProvider> {
                if (const std::optional<std::int32_t> rgb = rgbOf(v); rgb.has_value()) {
                    return IntProvider::constant(*rgb);
                }
                return intProvider(v);
            });
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            edit.colors = std::move(*parsed);
        }
        return done();
    }
    case Op::SetLore:
    case Op::SetWrittenBookPages:
    case Op::SetWritableBookPages: {
        const bool lore = edit.op == Op::SetLore;
        const std::vector<JsonValue>* values = asArray(object.find(lore ? "lore" : "pages"));
        if (values == nullptr) {
            return std::unexpected(missing(type + (lore ? " lore" : " pages")));
        }
        for (const JsonValue& value : *values) {
            Parsed<NbtTag> element = std::unexpected(invalid("lore line"));
            if (!lore) {
                element = filterable(value, edit.op == Op::SetWrittenBookPages);
            } else if (std::optional<NbtTag> line = nbtOf(value); line.has_value()) {
                element = std::move(*line);
            }
            if (!element.has_value()) {
                return std::unexpected(element.error());
            }
            edit.values.push_back(std::move(*element));
        }
        Parsed<ListOperation> operation = listOperation(object);
        if (!operation.has_value()) {
            return std::unexpected(operation.error());
        }
        edit.operation = *operation;
        if (lore && object.find("entity") != nullptr && !entityTargetOf(object.find("entity")).has_value()) {
            return std::unexpected(invalid("set_lore entity"));
        }
        return done();
    }
    case Op::SetBookCover: {
        if (const JsonValue* title = object.find("title"); title != nullptr) {
            Parsed<NbtTag> parsed = filterable(*title, false);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            edit.compound.set("title", std::move(*parsed));
        }
        if (const JsonValue* author = object.find("author"); author != nullptr) {
            if (author->get<std::string>() == nullptr) {
                return std::unexpected(invalid("book author"));
            }
            edit.compound.putString("author", *author->get<std::string>());
        }
        if (const std::optional<double> generation = asNumber(object.find("generation")); generation.has_value()) {
            if (*generation < 0 || *generation > kMaxGeneration) {
                return std::unexpected(invalid("book generation"));
            }
            edit.compound.putInt("generation", static_cast<std::int32_t>(*generation));
        }
        return done();
    }
    case Op::SetFireworkExplosion: {
        if (const std::optional<std::string> shape = asString(object.find("shape")); shape.has_value()) {
            if (std::ranges::find(kShapes, *shape) == kShapes.end()) {
                return std::unexpected(invalid("firework explosion shape"));
            }
            edit.compound.putString("shape", *shape);
        }
        for (const std::string_view key : {std::string_view{"colors"}, std::string_view{"fade_colors"}}) {
            if (const std::vector<JsonValue>* colors = asArray(object.find(key)); colors != nullptr) {
                std::vector<std::int32_t> ints;
                for (const JsonValue& color : *colors) {
                    ints.push_back(static_cast<std::int32_t>(asNumber(&color).value_or(0.0)));
                }
                edit.compound.putIntArray(key, std::move(ints));
            }
        }
        for (const auto& [from, to] : {std::pair{"trail", "has_trail"}, std::pair{"twinkle", "has_twinkle"}}) {
            if (const std::optional<bool> flag = asBool(object.find(from)); flag.has_value()) {
                edit.compound.putBoolean(to, *flag);
            }
        }
        return done();
    }
    case Op::SetFireworks: {
        if (const JsonObject* explosions = asObject(object.find("explosions")); explosions != nullptr) {
            auto parsed = standAlone<NbtTag>(*explosions, [](const JsonValue& v) { return explosion(v); });
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            edit.values = std::move(parsed->first);
            edit.operation = parsed->second;
            edit.hasOperation = true;
        }
        if (const std::optional<double> flight = asNumber(object.find("flight_duration")); flight.has_value()) {
            if (*flight < 0 || *flight > kMaxUnsignedByte) {
                return std::unexpected(invalid("flight_duration"));
            }
            edit.flightDuration = static_cast<std::int32_t>(*flight);
        }
        return done();
    }
    case Op::SetBannerPattern: {
        const std::vector<JsonValue>* patterns = asArray(object.find("patterns"));
        const std::optional<bool> append = asBool(object.find("append"));
        if (patterns == nullptr || !append.has_value()) {
            return std::unexpected(missing("set_banner_pattern patterns and append"));
        }
        std::vector<NbtTag> layers;
        for (const JsonValue& layer : *patterns) {
            const JsonObject* fields = layer.get<JsonObject>();
            std::optional<NbtTag> tag = nbtOf(layer);
            if (fields == nullptr || fields->find("pattern") == nullptr || !asString(fields->find("color")) ||
                !tag.has_value()) {
                return std::unexpected(invalid("banner pattern layer"));
            }
            layers.push_back(std::move(*tag));
        }
        wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
        for (NbtTag& layer : layers) {
            static_cast<void>(list.push(std::move(layer)));
        }
        edit.compound.set("patterns", NbtTag::ofList(std::move(list)));
        edit.append = *append;
        return done();
    }
    case Op::SetItem:
    case Op::SetLootTable: {
        const bool item = edit.op == Op::SetItem;
        const JsonValue* id = object.find(item ? "item" : "loot_table_id");
        // parity: `SetContainerLootTable.ID_ONLY_CODEC`.
        const std::optional<std::string> name = asString(id);
        const std::expected<Identifier, wire::common::IdentifierError> parsed =
            name.has_value() ? Identifier::parse(*name) : std::unexpected(wire::common::IdentifierError::EmptyPart);
        if (!parsed.has_value()) {
            return std::unexpected(missing(item ? "set_item item" : "set_loot_table loot_table_id"));
        }
        edit.id = *parsed;
        edit.seed = static_cast<std::int64_t>(asNumber(object.find("seed")).value_or(0.0));
        return done();
    }
    case Op::FillPlayerHead: {
        const std::optional<EntityTarget> target = entityTargetOf(object.find("entity"));
        if (!target.has_value()) {
            return std::unexpected(missing("fill_player_head entity"));
        }
        edit.entity = *target;
        return done();
    }
    case Op::SetAttributes: {
        const std::vector<JsonValue>* modifiers = asArray(object.find("modifiers"));
        if (modifiers == nullptr) {
            return std::unexpected(missing("set_attributes modifiers"));
        }
        for (const JsonValue& each : *modifiers) {
            const JsonObject* modifier = each.get<JsonObject>();
            const JsonValue* amount = modifier == nullptr ? nullptr : modifier->find("amount");
            if (amount == nullptr) {
                return std::unexpected(missing("attribute modifier amount"));
            }
            AttributeModifierSpec spec;
            spec.id = asString(modifier->find("id")).value_or("");
            spec.attribute = asString(modifier->find("attribute")).value_or("");
            spec.operation = asString(modifier->find("operation")).value_or("");
            if (!componentName(spec.id).has_value() || !componentName(spec.attribute).has_value() ||
                std::ranges::find(kOperations, spec.operation) == kOperations.end()) {
                return std::unexpected(invalid("attribute modifier"));
            }
            spec.id = *componentName(spec.id);
            spec.attribute = *componentName(spec.attribute);
            Parsed<FloatProvider> rolled = floatProvider(*amount);
            if (!rolled.has_value()) {
                return std::unexpected(rolled.error());
            }
            spec.amount = std::move(*rolled);
            // parity: `compactListCodec`, one slot group or a non-empty list of them.
            const JsonValue* slot = modifier->find("slot");
            if (const std::optional<std::string> one = asString(slot); one.has_value()) {
                spec.slots.push_back(*one);
            } else if (const std::vector<JsonValue>* many = asArray(slot); many != nullptr) {
                for (const JsonValue& group : *many) {
                    spec.slots.push_back(asString(&group).value_or(""));
                }
            }
            if (spec.slots.empty() || std::ranges::any_of(spec.slots, [](const std::string& group) {
                    return std::ranges::find(kSlotGroups, group) == kSlotGroups.end();
                })) {
                return std::unexpected(invalid("attribute modifier slot"));
            }
            edit.modifiers.push_back(std::move(spec));
        }
        edit.replace = asBool(object.find("replace")).value_or(true);
        return done();
    }
    case Op::SetContents:
    case Op::ModifyContents:
    default: {
        const std::optional<std::string> component = componentName(asString(object.find("component")).value_or(""));
        if (!component.has_value() || std::ranges::find(kContainers, *component) == kContainers.end()) {
            return std::unexpected(invalid(type + " component"));
        }
        edit.component = *component;
        if (edit.op == Op::ModifyContents) {
            const JsonValue* modifier = object.find("modifier");
            if (modifier == nullptr) {
                return std::unexpected(missing("modify_contents modifier"));
            }
            Parsed<LootFunction> parsed = function(*modifier);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            edit.modifier.push_back(std::move(*parsed));
            return done();
        }
        const std::vector<JsonValue>* entries = asArray(object.find("entries"));
        if (entries == nullptr) {
            return std::unexpected(missing("set_contents entries"));
        }
        for (const JsonValue& element : *entries) {
            const JsonObject* each = element.get<JsonObject>();
            if (each == nullptr) {
                return std::unexpected(missing("set_contents entry"));
            }
            Parsed<LootEntry> parsed = entry(*each);
            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }
            edit.entries.push_back(std::move(*parsed));
        }
        return done();
    }
    }
}

Parsed<std::vector<std::string>> Reader::tagValues(std::string_view registry, const std::string& id) {
    Parsed<JsonValue> json = named("tags/" + std::string{registry}, id);
    if (!json.has_value()) {
        return std::unexpected(json.error());
    }
    std::vector<std::string> values;
    const JsonObject* file = json->get<JsonObject>();
    const std::vector<JsonValue>* listed = file == nullptr ? nullptr : asArray(file->find("values"));
    if (listed == nullptr) {
        resolving_.pop_back();
        return std::unexpected(missing("tag values"));
    }
    for (const JsonValue& value : *listed) {
        const JsonObject* entryObject = value.get<JsonObject>();
        const std::optional<std::string> name =
            entryObject != nullptr ? asString(entryObject->find("id")) : asString(&value);
        const bool required = entryObject == nullptr || asBool(entryObject->find("required")).value_or(true);
        if (!name.has_value()) {
            resolving_.pop_back();
            return std::unexpected(missing("tag entry"));
        }
        if (name->starts_with('#')) {
            Parsed<std::vector<std::string>> nested = tagValues(registry, name->substr(1));
            if (!nested.has_value()) {
                if (!required) {
                    continue;
                }
                resolving_.pop_back();
                return nested;
            }
            for (std::string& member : *nested) {
                if (std::ranges::find(values, member) == values.end()) {
                    values.push_back(std::move(member));
                }
            }
            continue;
        }
        const std::expected<Identifier, wire::common::IdentifierError> member = Identifier::parse(*name);
        if (!member.has_value()) {
            resolving_.pop_back();
            return std::unexpected(invalid("tag entry"));
        }
        // parity: `TagEntry.required`, an optional element that is not there is skipped.
        if (!required && (references_ == nullptr || !references_->read(registry, *member).has_value())) {
            continue;
        }
        if (std::ranges::find(values, member->toString()) == values.end()) {
            values.push_back(member->toString());
        }
    }
    resolving_.pop_back();
    return values;
}

Parsed<SlotSource> Reader::slotSource(const JsonValue& value) {
    SlotSource built;
    if (const std::optional<std::string> name = asString(&value); name.has_value()) {
        // parity: `SlotSources.LIST_CODEC` accepts a `#tag`, which is a group of its members.
        if (name->starts_with('#')) {
            Parsed<std::vector<std::string>> members = tagValues("slot_source", name->substr(1));
            if (!members.has_value()) {
                return std::unexpected(members.error());
            }
            built.kind = SlotSource::Kind::Group;
            for (const std::string& member : *members) {
                Parsed<SlotSource> term = slotSource(JsonValue::of(member));
                if (!term.has_value()) {
                    return term;
                }
                built.terms.push_back(std::move(*term));
            }
            return built;
        }
        Parsed<JsonValue> json = named("slot_source", *name);
        if (!json.has_value()) {
            return std::unexpected(json.error());
        }
        Parsed<SlotSource> resolved = slotSource(*json);
        resolving_.pop_back();
        return resolved;
    }
    // parity: `GroupSlotSource.INLINE_CODEC`, a list is a group.
    if (const std::vector<JsonValue>* list = value.get<std::vector<JsonValue>>(); list != nullptr) {
        built.kind = SlotSource::Kind::Group;
        for (const JsonValue& element : *list) {
            Parsed<SlotSource> term = slotSource(element);
            if (!term.has_value()) {
                return term;
            }
            built.terms.push_back(std::move(*term));
        }
        return built;
    }
    const JsonObject* object = value.get<JsonObject>();
    const std::optional<std::string> type = object == nullptr ? std::nullopt : typeOf(*object);
    if (!type.has_value()) {
        return std::unexpected(missing("slot source type"));
    }
    if (*type == "minecraft:empty") {
        return built;
    }
    if (*type == "minecraft:group") {
        const JsonValue* terms = object->find("terms");
        if (terms == nullptr) {
            return std::unexpected(missing("slot source terms"));
        }
        Parsed<SlotSource> group = slotSource(terms->get<std::vector<JsonValue>>() != nullptr
                                                  ? *terms
                                                  : JsonValue::ofArray(std::vector<JsonValue>{*terms}));
        return group;
    }
    if (*type == "minecraft:slot_range") {
        built.kind = SlotSource::Kind::Range;
        built.slots = asString(object->find("slots")).value_or("");
        if (const std::optional<std::string> source = asString(object->find("source")); source.has_value()) {
            if (*source != "block_entity" && *source != "container" && !entityTargetOf(object->find("source"))) {
                return std::unexpected(invalid("slot source"));
            }
            built.source = *source;
        }
        if (!isSlotRange(built.slots)) {
            return std::unexpected(invalid("slot range: " + built.slots));
        }
        return built;
    }
    static const std::map<std::string, SlotSource::Kind, std::less<>> kTransformed{
        {"minecraft:filtered", SlotSource::Kind::Filtered},
        {"minecraft:limit_slots", SlotSource::Kind::Limit},
        {"minecraft:contents", SlotSource::Kind::Contents},
    };
    const auto transformed = kTransformed.find(*type);
    const JsonValue* inner = object->find("slot_source");
    if (transformed == kTransformed.end()) {
        return std::unexpected(LootTableParseError{"Unsupported slot source type: " + *type});
    }
    if (inner == nullptr) {
        return std::unexpected(missing("slot_source"));
    }
    Parsed<SlotSource> child = slotSource(*inner);
    if (!child.has_value()) {
        return child;
    }
    built.kind = transformed->second;
    built.child = std::make_shared<const SlotSource>(std::move(*child));
    if (built.kind == SlotSource::Kind::Filtered) {
        const JsonObject* filter = asObject(object->find("item_filter"));
        if (filter == nullptr) {
            return std::unexpected(missing("item_filter"));
        }
        built.filter = parseItemPredicate(*filter);
    } else if (built.kind == SlotSource::Kind::Limit) {
        const std::optional<double> limit = asNumber(object->find("limit"));
        if (!limit.has_value() || *limit < 1) {
            return std::unexpected(invalid("slot limit"));
        }
        built.limit = static_cast<std::int32_t>(*limit);
    } else {
        const std::optional<std::string> component = componentName(asString(object->find("component")).value_or(""));
        if (!component.has_value() || std::ranges::find(kContainers, *component) == kContainers.end()) {
            return std::unexpected(invalid("contents component"));
        }
        built.component = *component;
    }
    return built;
}

} // namespace basalt::server::world::loot::lootTableJson::parts
