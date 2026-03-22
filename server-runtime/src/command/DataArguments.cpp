#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/DataArguments.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>

#include <algorithm>
#include <expected>
#include <string>
#include <utility>

namespace basalt::server::runtime::command {
namespace {

namespace parserId = wire::protocol::play::argumentType;

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;
using wire::protocol::play::ArgumentParser;

/// parity: `IdentifierParseRule`, the characters an unquoted identifier may hold.
[[nodiscard]] bool identifierChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':' || c == '/';
}

class ResourceOrInlineArgument final : public ArgumentType {
public:
    explicit ResourceOrInlineArgument(std::int32_t parser) : parser_(parser) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        std::int32_t end = start;
        while (reader.canRead(end - start + 1) && identifierChar(reader.peek(end - start))) {
            ++end;
        }
        const bool atBoundary = !reader.canRead(end - start + 1) || reader.peek(end - start) == ' ';
        if (end > start && atBoundary) {
            const std::string text =
                reader.string().substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start));
            if (const auto id = Identifier::parse(text); id.has_value()) {
                reader.setCursor(end);
                return ArgumentValue::of(ResourceOrInline{.id = *id});
            }
        }
        const std::expected<wire::nbt::SnbtValue, wire::nbt::NbtError> value =
            wire::nbt::snbt::parseFrom(reader.string(), static_cast<std::size_t>(start));
        if (!value.has_value()) {
            return std::unexpected{reader.syntaxError("Expected an id or an inline value")};
        }
        reader.setCursor(static_cast<std::int32_t>(value->position));
        return ArgumentValue::of(ResourceOrInline{.inlineValue = jsonOf(value->tag)});
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parser_}; }

private:
    std::int32_t parser_;
};

class SlotSourceArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        std::int32_t end = start;
        while (reader.canRead(end - start + 1) && reader.peek(end - start) != ' ') {
            ++end;
        }
        const std::string name =
            reader.string().substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start));
        const std::vector<SlotSourceValue>& ranges = slotRanges();
        const auto found = std::ranges::find(ranges, name, &SlotSourceValue::name);
        if (found == ranges.end()) {
            return std::unexpected{
                reader.syntaxError("Slot sources other than slot names are not supported: '" + name + "'")};
        }
        reader.setCursor(end);
        return ArgumentValue::of(*found);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kSlotSource}; }
};

class SwingAnimationArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string name = reader.readUnquotedString();
        if (name != "none" && name != "whack" && name != "stab") {
            reader.setCursor(start);
            return std::unexpected{
                CommandSyntaxError{.component = wire::text::Component::translatable(
                                       "argument.swing_animation.invalid", {wire::text::Component::text(name)})}};
        }
        return ArgumentValue::of(name);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kSwingAnimation}; }
};

void addRange(std::vector<SlotSourceValue>& into, const std::string& prefix, std::int32_t offset, std::int32_t size) {
    SlotSourceValue all{.name = prefix + "*"};
    for (std::int32_t index = 0; index < size; ++index) {
        into.push_back(SlotSourceValue{.name = prefix + std::to_string(index), .slots = {offset + index}});
        all.slots.push_back(offset + index);
    }
    into.push_back(std::move(all));
}

} // namespace

std::shared_ptr<const ArgumentType> lootModifierOrInlineArgument() {
    static const auto type = std::make_shared<const ResourceOrInlineArgument>(parserId::kLootModifier);
    return type;
}

std::shared_ptr<const ArgumentType> contextIntProviderArgument() {
    static const auto type = std::make_shared<const ResourceOrInlineArgument>(parserId::kContextIntProvider);
    return type;
}

std::shared_ptr<const ArgumentType> contextFloatProviderArgument() {
    static const auto type = std::make_shared<const ResourceOrInlineArgument>(parserId::kContextFloatProvider);
    return type;
}

std::shared_ptr<const ArgumentType> slotSourceArgument() {
    static const auto type = std::make_shared<const SlotSourceArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> swingAnimationArgument() {
    static const auto type = std::make_shared<const SwingAnimationArgumentType>();
    return type;
}

const std::vector<SlotSourceValue>& slotRanges() {
    // parity: `SlotRanges.SLOTS`, the equipment slots at `EquipmentSlot.getIndex` of 98, 100 and 105.
    static const std::vector<SlotSourceValue> ranges = [] {
        std::vector<SlotSourceValue> built;
        built.push_back({.name = "contents", .slots = {0}});
        addRange(built, "container.", 0, 54);
        addRange(built, "hotbar.", 0, 9);
        addRange(built, "inventory.", 9, 27);
        addRange(built, "enderchest.", 200, 27);
        addRange(built, "mob.inventory.", 300, 8);
        addRange(built, "horse.", 500, 15);
        constexpr std::int32_t kMainhand = 98;
        constexpr std::int32_t kOffhand = 99;
        built.push_back({.name = "weapon", .slots = {kMainhand}});
        built.push_back({.name = "weapon.mainhand", .slots = {kMainhand}});
        built.push_back({.name = "weapon.offhand", .slots = {kOffhand}});
        built.push_back({.name = "weapon.*", .slots = {kMainhand, kOffhand}});
        constexpr std::int32_t kHead = 103;
        constexpr std::int32_t kChest = 102;
        constexpr std::int32_t kLegs = 101;
        constexpr std::int32_t kFeet = 100;
        constexpr std::int32_t kBody = 105;
        built.push_back({.name = "armor.head", .slots = {kHead}});
        built.push_back({.name = "armor.chest", .slots = {kChest}});
        built.push_back({.name = "armor.legs", .slots = {kLegs}});
        built.push_back({.name = "armor.feet", .slots = {kFeet}});
        built.push_back({.name = "armor.body", .slots = {kBody}});
        built.push_back({.name = "armor.*", .slots = {kHead, kChest, kLegs, kFeet, kBody}});
        built.push_back({.name = "saddle", .slots = {106}});
        built.push_back({.name = "horse.chest", .slots = {499}});
        built.push_back({.name = "player.cursor", .slots = {499}});
        addRange(built, "player.crafting.", 500, 4);
        return built;
    }();
    return ranges;
}

JsonValue jsonOf(const wire::nbt::NbtTag& tag) {
    return std::visit(
        [](const auto& value) -> JsonValue {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::int8_t>) {
                return value == 0 || value == 1 ? JsonValue::of(value == 1)
                                                : JsonValue::of(static_cast<std::int64_t>(value));
            } else if constexpr (std::is_same_v<T, std::int16_t> || std::is_same_v<T, std::int32_t> ||
                                 std::is_same_v<T, std::int64_t>) {
                return JsonValue::of(static_cast<std::int64_t>(value));
            } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
                return JsonValue::of(static_cast<double>(value));
            } else if constexpr (std::is_same_v<T, std::string>) {
                return JsonValue::of(value);
            } else if constexpr (std::is_same_v<T, wire::nbt::NbtList>) {
                std::vector<JsonValue> items;
                for (const wire::nbt::NbtTag& element : value.elements()) {
                    items.push_back(jsonOf(element));
                }
                return JsonValue::ofArray(std::move(items));
            } else if constexpr (std::is_same_v<T, wire::nbt::NbtCompound>) {
                JsonObject object;
                for (const std::string& name : value.names()) {
                    object.set(name, jsonOf(*value.find(name)));
                }
                return JsonValue::ofObject(std::move(object));
            } else if constexpr (std::is_same_v<T, std::monostate>) {
                return JsonValue{};
            } else {
                std::vector<JsonValue> items;
                for (const auto element : value) {
                    items.push_back(JsonValue::of(static_cast<std::int64_t>(element)));
                }
                return JsonValue::ofArray(std::move(items));
            }
        },
        tag.storage());
}

} // namespace basalt::server::runtime::command
