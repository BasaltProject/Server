#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace basalt::server::runtime::command {
namespace {

namespace parserId = wire::protocol::play::argumentType;

using wire::protocol::play::ArgumentParser;

/// One of a fixed set of words, kept as written. Four of the ten arguments here are exactly this.
class WordFromSetArgument final : public ArgumentType {
public:
    WordFromSetArgument(std::int32_t parser, std::vector<std::string> names, std::string message)
        : parser_(parser), names_(std::move(names)), message_(std::move(message)) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string name = reader.readUnquotedString();
        if (std::ranges::find(names_, name) == names_.end()) {
            reader.setCursor(start);
            return std::unexpected{reader.syntaxError(message_ + " '" + name + "'")};
        }
        return ArgumentValue::of(name);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parser_}; }

private:
    std::int32_t parser_;
    std::vector<std::string> names_;
    std::string message_;
};

/// An id read exactly as a resource location, declared as its own parser so the client knows what
/// it is completing.
class IdArgument final : public ArgumentType {
public:
    explicit IdArgument(std::int32_t parser) : parser_(parser) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource& source,
                                              const CommandEnvironment& environment) const override {
        return resourceLocationArgument()->parse(reader, source, environment);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parser_}; }

private:
    std::int32_t parser_;
};

class EntityAnchorArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string name = reader.readUnquotedString();
        if (name == "feet") {
            return ArgumentValue::of(EntityAnchor::Feet);
        }
        if (name == "eyes") {
            return ArgumentValue::of(EntityAnchor::Eyes);
        }
        reader.setCursor(start);
        return std::unexpected{reader.syntaxError("Invalid entity anchor")};
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kEntityAnchor}; }
};

class SwizzleArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        static constexpr std::string_view kMessage = "Invalid swizzle, expected combination of 'x', 'y' and 'z'";
        const std::int32_t start = reader.cursor();
        std::set<char> axes;
        while (reader.canRead() && reader.peek() != ' ') {
            const char axis = reader.read();
            if ((axis != 'x' && axis != 'y' && axis != 'z') || !axes.insert(axis).second) {
                reader.setCursor(start);
                return std::unexpected{reader.syntaxError(std::string{kMessage})};
            }
        }
        if (axes.empty()) {
            reader.setCursor(start);
            return std::unexpected{reader.syntaxError(std::string{kMessage})};
        }
        return ArgumentValue::of(axes);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kSwizzle}; }
};

/// A whole word, which is what `item_slots` and the two ranges keep.
class RawWordArgument final : public ArgumentType {
public:
    RawWordArgument(std::int32_t parser, bool quotable) : parser_(parser), quotable_(quotable) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource& source,
                                              const CommandEnvironment& environment) const override {
        if (quotable_) {
            const Parsed<std::string> text = reader.readString();
            if (!text.has_value()) {
                return std::unexpected{text.error()};
            }
            return ArgumentValue::of(*text);
        }
        return intRangeArgument()->parse(reader, source, environment);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parser_}; }

private:
    std::int32_t parser_;
    bool quotable_;
};

class UuidArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        static constexpr std::size_t kLength = 36;
        const std::int32_t start = reader.cursor();
        while (reader.canRead() && reader.peek() != ' ') {
            reader.skip();
        }
        const std::string text =
            reader.string().substr(static_cast<std::size_t>(start), static_cast<std::size_t>(reader.cursor() - start));
        if (text.size() != kLength || std::ranges::count(text, '-') != 4) {
            reader.setCursor(start);
            return std::unexpected{reader.syntaxError("Invalid UUID")};
        }
        return ArgumentValue::of(text);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kUuid}; }
};

class StyleArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource& source,
                                              const CommandEnvironment& environment) const override {
        return nbtCompoundArgument()->parse(reader, source, environment);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kStyle}; }
};

} // namespace

std::shared_ptr<const ArgumentType> entityAnchorArgument() {
    static const auto type = std::make_shared<const EntityAnchorArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> swizzleArgument() {
    static const auto type = std::make_shared<const SwizzleArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> heightmapArgument() {
    static const auto type = std::make_shared<const WordFromSetArgument>(
        parserId::kHeightmap,
        std::vector<std::string>{"world_surface", "motion_blocking", "motion_blocking_no_leaves", "ocean_floor"},
        "Unknown heightmap type");
    return type;
}

std::shared_ptr<const ArgumentType> itemSlotsArgument() {
    static const auto type = std::make_shared<const RawWordArgument>(parserId::kItemSlots, true);
    return type;
}

std::shared_ptr<const ArgumentType> lootPredicateArgument() {
    static const auto type = std::make_shared<const IdArgument>(parserId::kLootPredicate);
    return type;
}

std::shared_ptr<const ArgumentType> lootTableArgument() {
    static const auto type = std::make_shared<const IdArgument>(parserId::kLootTable);
    return type;
}

std::shared_ptr<const ArgumentType> lootModifierArgument() {
    static const auto type = std::make_shared<const IdArgument>(parserId::kLootModifier);
    return type;
}

std::shared_ptr<const ArgumentType> floatRangeArgument() {
    static const auto type = std::make_shared<const RawWordArgument>(parserId::kFloatRange, false);
    return type;
}

std::shared_ptr<const ArgumentType> uuidArgument() {
    static const auto type = std::make_shared<const UuidArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> styleArgument() {
    static const auto type = std::make_shared<const StyleArgumentType>();
    return type;
}

} // namespace basalt::server::runtime::command
