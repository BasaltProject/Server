#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::command {
namespace {

namespace parserId = wire::protocol::play::argumentType;
/// `world::` inside this namespace is `runtime::world`, and the two transforms live in the world
/// module's own.
namespace structure = server::world::structure;

using wire::protocol::play::ArgumentParser;
using wire::protocol::play::DisplaySlot;
using wire::protocol::play::TeamColor;
using wire::text::Component;

/// A registry entry read exactly as a resource location, differing only in what the client is told.
class RegistryArgument final : public ArgumentType {
public:
    RegistryArgument(std::int32_t parser, std::string registry) : parser_(parser), registry_(std::move(registry)) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource& source,
                                              const CommandEnvironment& environment) const override {
        return resourceLocationArgument()->parse(reader, source, environment);
    }

    [[nodiscard]] ArgumentParser wire() const override {
        return registry_.empty() ? ArgumentParser{parser_}
                                 : ArgumentParser{parser_, ArgumentParser::RegistryProperties{.registry = registry_}};
    }

private:
    std::int32_t parser_;
    std::string registry_;
};

/// A name the client already keeps a list of: read as a brigadier string, declared as its own
/// parser so the client completes it without asking.
class NameArgument final : public ArgumentType {
public:
    explicit NameArgument(std::int32_t parser) : parser_(parser) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const Parsed<std::string> text = reader.readString();
        if (!text.has_value()) {
            return std::unexpected{text.error()};
        }
        return ArgumentValue::of(*text);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parser_}; }

private:
    std::int32_t parser_;
};

/// Everything up to the next space, which is how the three word-shaped parsers below read.
[[nodiscard]] std::string readWord(StringReader& reader) {
    const std::int32_t start = reader.cursor();
    while (reader.canRead() && std::isspace(static_cast<unsigned char>(reader.peek())) == 0) {
        reader.skip();
    }
    return reader.string().substr(static_cast<std::size_t>(start), static_cast<std::size_t>(reader.cursor() - start));
}

class IntRangeArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string text = readWord(reader);
        if (text.empty()) {
            reader.setCursor(start);
            return std::unexpected{reader.syntaxError("Expected value or range of values")};
        }
        return ArgumentValue::of(text);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kIntRange}; }
};

class HexColorArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        std::string text = readWord(reader);
        const std::string_view digits = std::string_view{text}.substr(text.starts_with('#') ? 1 : 0);
        std::int32_t value = 0;
        const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
        if (digits.empty() || result.ec != std::errc{} || result.ptr != digits.data() + digits.size()) {
            reader.setCursor(start);
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("argument.color.invalid", {Component::text(std::string{digits})}),
                .input = reader.string(),
                .cursor = reader.cursor()}};
        }
        return ArgumentValue::of(value);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kHexColor}; }
};

class TeamColorArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string name = reader.readUnquotedString();
        const std::optional<TeamColor> found = wire::protocol::play::teamColorByName(name);
        if (!found.has_value()) {
            reader.setCursor(start);
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("argument.color.invalid", {Component::text(name)}),
                .input = reader.string(),
                .cursor = reader.cursor()}};
        }
        return ArgumentValue::of(*found);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kTeamColor}; }
};

class ScoreboardSlotArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        static constexpr std::array<std::pair<std::string_view, DisplaySlot>, 3> kSlots{
            {{"list", DisplaySlot::List}, {"sidebar", DisplaySlot::Sidebar}, {"below_name", DisplaySlot::BelowName}}};
        const std::int32_t start = reader.cursor();
        const std::string name = reader.readUnquotedString();
        const auto found = std::ranges::find_if(kSlots, [&name](const auto& slot) { return slot.first == name; });
        if (found == kSlots.end()) {
            reader.setCursor(start);
            // parity: vanilla also takes `sidebar.team.<colour>`, which needs per-team display slots
            // this build has not got. Refused by name rather than drawn in the wrong slot.
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("argument.scoreboardDisplaySlot.invalid", {Component::text(name)}),
                .input = reader.string(),
                .cursor = reader.cursor()}};
        }
        return ArgumentValue::of(found->second);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kScoreboardSlot}; }
};

/// The two `/place template` modifiers, which read the same way and differ only in their table.
template <typename T, std::size_t N>
class TransformArgument final : public ArgumentType {
public:
    TransformArgument(std::int32_t parser, std::array<T, N> values, std::string noun)
        : parser_(parser), values_(values), noun_(std::move(noun)) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string name = reader.readUnquotedString();
        const auto found =
            std::ranges::find_if(values_, [&name](T value) { return structure::serializedName(value) == name; });
        if (found == values_.end()) {
            reader.setCursor(start);
            return std::unexpected{reader.syntaxError("Invalid " + noun_ + " '" + name + "'")};
        }
        return ArgumentValue::of(*found);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parser_}; }

private:
    std::int32_t parser_;
    std::array<T, N> values_;
    std::string noun_;
};

} // namespace

std::shared_ptr<const ArgumentType> resourceArgument(std::string registry) {
    return std::make_shared<const RegistryArgument>(parserId::kResource, std::move(registry));
}

std::shared_ptr<const ArgumentType> resourceKeyArgument(std::string registry) {
    return std::make_shared<const RegistryArgument>(parserId::kResourceKey, std::move(registry));
}

std::shared_ptr<const ArgumentType> resourceOrTagArgument(std::string registry) {
    return std::make_shared<const RegistryArgument>(parserId::kResourceOrTag, std::move(registry));
}

std::shared_ptr<const ArgumentType> resourceOrTagKeyArgument(std::string registry) {
    return std::make_shared<const RegistryArgument>(parserId::kResourceOrTagKey, std::move(registry));
}

std::shared_ptr<const ArgumentType> resourceSelectorArgument(std::string registry) {
    return std::make_shared<const RegistryArgument>(parserId::kResourceSelector, std::move(registry));
}

std::shared_ptr<const ArgumentType> functionArgument() {
    static const auto type = std::make_shared<const RegistryArgument>(parserId::kFunction, std::string{});
    return type;
}

std::shared_ptr<const ArgumentType> dialogArgument() {
    static const auto type = std::make_shared<const RegistryArgument>(parserId::kDialog, std::string{});
    return type;
}

std::shared_ptr<const ArgumentType> objectiveArgument() {
    static const auto type = std::make_shared<const NameArgument>(parserId::kObjective);
    return type;
}

std::shared_ptr<const ArgumentType> objectiveCriteriaArgument() {
    static const auto type = std::make_shared<const NameArgument>(parserId::kObjectiveCriteria);
    return type;
}

std::shared_ptr<const ArgumentType> teamArgument() {
    static const auto type = std::make_shared<const NameArgument>(parserId::kTeam);
    return type;
}

std::shared_ptr<const ArgumentType> itemSlotArgument() {
    static const auto type = std::make_shared<const NameArgument>(parserId::kItemSlot);
    return type;
}

std::shared_ptr<const ArgumentType> intRangeArgument() {
    static const auto type = std::make_shared<const IntRangeArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> hexColorArgument() {
    static const auto type = std::make_shared<const HexColorArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> teamColorArgument() {
    static const auto type = std::make_shared<const TeamColorArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> scoreboardSlotArgument() {
    static const auto type = std::make_shared<const ScoreboardSlotArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> templateRotationArgument() {
    using structure::StructureRotation;
    static const auto type = std::make_shared<const TransformArgument<StructureRotation, 4>>(
        parserId::kTemplateRotation,
        std::array<StructureRotation, 4>{StructureRotation::None, StructureRotation::Clockwise90,
                                         StructureRotation::Clockwise180, StructureRotation::Counterclockwise90},
        "rotation");
    return type;
}

std::shared_ptr<const ArgumentType> templateMirrorArgument() {
    using structure::StructureMirror;
    static const auto type = std::make_shared<const TransformArgument<StructureMirror, 3>>(
        parserId::kTemplateMirror,
        std::array<StructureMirror, 3>{StructureMirror::None, StructureMirror::LeftRight, StructureMirror::FrontBack},
        "mirror");
    return type;
}

} // namespace basalt::server::runtime::command
