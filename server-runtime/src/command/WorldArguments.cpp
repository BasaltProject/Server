#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::command {
namespace {

namespace parserId = wire::protocol::play::argumentType;
namespace reg = server::registry;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::protocol::play::ArgumentParser;
using wire::text::Component;

[[nodiscard]] CommandSyntaxError errorAt(StringReader& reader, std::int32_t cursor, Component message) {
    reader.setCursor(cursor);
    return CommandSyntaxError{.component = std::move(message), .input = reader.string(), .cursor = reader.cursor()};
}

/// One SNBT value, read from where the reader stands.
[[nodiscard]] Parsed<NbtTag> parseTag(StringReader& reader) {
    const std::int32_t start = reader.cursor();
    const std::expected<wire::nbt::SnbtValue, wire::nbt::NbtError> value =
        wire::nbt::snbt::parseFrom(reader.string(), static_cast<std::size_t>(start));
    if (!value.has_value()) {
        reader.setCursor(start);
        return std::unexpected{reader.syntaxError("Invalid NBT")};
    }
    reader.setCursor(static_cast<std::int32_t>(value->position));
    return value->tag;
}

/// A block or `#tag` with the properties written next to it, before anything is resolved.
struct ParsedBlock {
    Identifier id;
    std::optional<Identifier> tag;
    std::map<std::string, std::string> properties;
    std::int32_t start = 0;

    /// Resolves to a concrete state, or fails naming what could not be resolved.
    [[nodiscard]] Parsed<BlockInput> asState(StringReader& reader) const {
        const std::optional<std::int32_t> stateId = reg::blocks::stateIdOf(id, properties);
        if (!stateId.has_value()) {
            std::string written;
            for (const auto& [key, value] : properties) {
                written += (written.empty() ? "" : ",") + key + "=" + value;
            }
            return std::unexpected{
                errorAt(reader, start,
                        properties.empty()
                            ? Component::translatable("argument.block.id.invalid", {Component::text(id.toString())})
                            : Component::translatable("argument.block.property.unknown",
                                                      {Component::text(id.toString()), Component::text(written)}))};
        }
        return BlockInput{.block = id, .stateId = *stateId, .properties = properties};
    }
};

/// parity: `BlockStateParser`, minus its NBT tail, which only block entities use.
[[nodiscard]] Parsed<ParsedBlock> parseBlock(StringReader& reader, bool allowTag) {
    const std::int32_t start = reader.cursor();
    const bool isTag = reader.canRead() && reader.peek() == '#';
    if (isTag) {
        if (!allowTag) {
            return std::unexpected{reader.syntaxError("Tags aren't allowed here, only actual blocks")};
        }
        reader.skip();
    }
    const std::string name = reader.readIdentifier();
    if (name.empty()) {
        reader.setCursor(start);
        return std::unexpected{reader.syntaxError("Expected block")};
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(name);
    if (!id.has_value()) {
        reader.setCursor(start);
        return std::unexpected{reader.syntaxError("Expected block")};
    }
    std::map<std::string, std::string> properties;
    if (reader.canRead() && reader.peek() == '[') {
        reader.skip();
        reader.skipWhitespace();
        while (reader.canRead() && reader.peek() != ']') {
            reader.skipWhitespace();
            const std::string key = reader.readUnquotedString();
            reader.skipWhitespace();
            if (const Parsed<void> equals = reader.expect('='); !equals.has_value()) {
                return std::unexpected{equals.error()};
            }
            reader.skipWhitespace();
            properties.insert_or_assign(key, reader.readUnquotedString());
            reader.skipWhitespace();
            if (reader.canRead() && reader.peek() == ',') {
                reader.skip();
                continue;
            }
            break;
        }
        if (const Parsed<void> closed = reader.expect(']'); !closed.has_value()) {
            return std::unexpected{closed.error()};
        }
    }
    return ParsedBlock{.id = *id,
                       .tag = isTag ? std::optional{*id} : std::nullopt,
                       .properties = std::move(properties),
                       .start = start};
}

/// `[a=1, b={c:2}]`, each value SNBT. Keys are normalised to their full id, as the wire writes them.
[[nodiscard]] Parsed<NbtCompound> parseComponents(StringReader& reader) {
    NbtCompound components;
    if (!reader.canRead() || reader.peek() != '[') {
        return components;
    }
    const std::int32_t start = reader.cursor();
    reader.skip();
    reader.skipWhitespace();
    while (reader.canRead() && reader.peek() != ']') {
        reader.skipWhitespace();
        const std::expected<Identifier, wire::common::IdentifierError> name =
            Identifier::parse(reader.readIdentifier());
        if (!name.has_value()) {
            return std::unexpected{errorAt(reader, start, Component::text("Unknown item component."))};
        }
        const std::string key = name->toString();
        reader.skipWhitespace();
        if (const Parsed<void> equals = reader.expect('='); !equals.has_value()) {
            return std::unexpected{equals.error()};
        }
        reader.skipWhitespace();
        if (!item::itemComponentCodecs::exists(key)) {
            return std::unexpected{errorAt(reader, start, Component::text("Unknown item component '" + key + "'."))};
        }
        const Parsed<NbtTag> value = parseTag(reader);
        if (!value.has_value()) {
            return std::unexpected{value.error()};
        }
        components.set(key, *value);
        reader.skipWhitespace();
        if (reader.canRead() && reader.peek() == ',') {
            reader.skip();
            continue;
        }
        break;
    }
    if (const Parsed<void> closed = reader.expect(']'); !closed.has_value()) {
        return std::unexpected{closed.error()};
    }
    return components;
}

class BlockStateArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const Parsed<ParsedBlock> parsed = parseBlock(reader, false);
        if (!parsed.has_value()) {
            return std::unexpected{parsed.error()};
        }
        const Parsed<BlockInput> state = parsed->asState(reader);
        if (!state.has_value()) {
            return std::unexpected{state.error()};
        }
        return ArgumentValue::of(*state);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kBlockState}; }
};

class BlockPredicateArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment& environment) const override {
        const Parsed<ParsedBlock> parsed = parseBlock(reader, true);
        if (!parsed.has_value()) {
            return std::unexpected{parsed.error()};
        }
        // A bare block still has to resolve; a tag is checked when it is matched against.
        if (!parsed->tag.has_value()) {
            if (const Parsed<BlockInput> state = parsed->asState(reader); !state.has_value()) {
                return std::unexpected{state.error()};
            }
        }
        return ArgumentValue::of(BlockPredicateInput{parsed->tag.has_value() ? std::nullopt : std::optional{parsed->id},
                                                     parsed->tag, parsed->properties,
                                                     environment.options().blockTagMembers});
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kBlockPredicate}; }
};

class ItemPredicateArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment& environment) const override {
        const Parsed<ParsedBlock> parsed = parseBlock(reader, true);
        if (!parsed.has_value()) {
            return std::unexpected{parsed.error()};
        }
        if (!parsed->tag.has_value() && reg::items::registry().byName(parsed->id) == nullptr) {
            return std::unexpected{
                errorAt(reader, parsed->start,
                        Component::translatable("argument.item.id.invalid", {Component::text(parsed->id.toString())}))};
        }
        return ArgumentValue::of(ItemPredicateInput{parsed->tag.has_value() ? std::nullopt : std::optional{parsed->id},
                                                    parsed->tag, environment.options().itemTagMembers});
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kItemPredicate}; }
};

class ItemArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(reader.readIdentifier());
        if (!id.has_value() || reg::items::registry().byName(*id) == nullptr) {
            return std::unexpected{
                errorAt(reader, start,
                        Component::translatable("argument.item.id.invalid",
                                                {Component::text(std::string{reader.remaining()})}))};
        }
        const Parsed<NbtCompound> components = parseComponents(reader);
        if (!components.has_value()) {
            return std::unexpected{components.error()};
        }
        return ArgumentValue::of(ParsedItem{.id = *id, .components = *components});
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kItemStack}; }
};

/// Vanilla accepts any unquoted word plus the sigils fake players conventionally start with.
[[nodiscard]] std::string readHolderName(StringReader& reader) {
    const std::int32_t start = reader.cursor();
    while (reader.canRead() && std::isspace(static_cast<unsigned char>(reader.peek())) == 0) {
        reader.skip();
    }
    return reader.string().substr(static_cast<std::size_t>(start), static_cast<std::size_t>(reader.cursor() - start));
}

class ScoreHolderArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource& source,
                                              const CommandEnvironment& environment) const override {
        std::vector<std::string> named;
        if (reader.canRead() && reader.peek() == '@') {
            const Parsed<ArgumentValue> matched = playersArgument(false)->parse(reader, source, environment);
            if (!matched.has_value()) {
                return std::unexpected{matched.error()};
            }
            const auto* players = matched->as<std::vector<player::PlayerSession*>>();
            if (players != nullptr) {
                for (const player::PlayerSession* one : *players) {
                    named.push_back(one->name());
                }
            }
            return ArgumentValue::of(named);
        }
        if (reader.canRead() && reader.peek() == '*') {
            reader.skip();
            for (const player::PlayerSession* one : environment.players().snapshot()) {
                named.push_back(one->name());
            }
            return ArgumentValue::of(named);
        }
        const std::int32_t start = reader.cursor();
        const std::string name = readHolderName(reader);
        if (name.empty()) {
            reader.setCursor(start);
            return std::unexpected{reader.syntaxError("Expected score holder")};
        }
        named.push_back(name);
        return ArgumentValue::of(named);
    }

    [[nodiscard]] ArgumentParser wire() const override {
        return ArgumentParser{parserId::kScoreHolder, ArgumentParser::ScoreHolderProperties{.multiple = true}};
    }

    /// parity: `ScoreHolderArgument.SUGGEST_SCORE_HOLDERS`, which is where fake players come from.
    [[nodiscard]] SuggestionProvider suggestions() const override {
        return SuggestionProvider{[](const SuggestionContext& context) {
            const scoreboard::Scoreboard* board = context.environment().options().scoreboard;
            return context.matching(board == nullptr ? std::vector<std::string>{} : board->holders());
        }};
    }

    /// Vanilla flags every node that takes one of these, so the client asks rather than guessing.
    [[nodiscard]] std::optional<std::string> clientSuggestions() const override {
        return std::string{suggestionProviders::kAskServer};
    }
};

class OperationArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        static constexpr std::array<std::string_view, 9> kOperations{"=", "+=", "-=", "*=", "/=", "%=", "<", ">", "><"};
        const std::int32_t start = reader.cursor();
        const std::string text = readHolderName(reader);
        if (std::ranges::find(kOperations, text) == kOperations.end()) {
            return std::unexpected{errorAt(reader, start, Component::translatable("arguments.operation.invalid"))};
        }
        return ArgumentValue::of(text);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kOperation}; }
};

class NbtCompoundArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const Parsed<NbtTag> tag = parseTag(reader);
        if (!tag.has_value()) {
            return std::unexpected{tag.error()};
        }
        const auto* compound = tag->get<NbtCompound>();
        if (compound == nullptr) {
            return std::unexpected{reader.syntaxError("Expected a compound")};
        }
        return ArgumentValue::of(*compound);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kNbtCompoundTag}; }
};

class NbtTagArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const Parsed<NbtTag> tag = parseTag(reader);
        if (!tag.has_value()) {
            return std::unexpected{tag.error()};
        }
        return ArgumentValue::of(*tag);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kNbtTag}; }
};

class NbtPathArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        bool quoted = false;
        while (reader.canRead()) {
            const char next = reader.peek();
            if (next == '"') {
                quoted = !quoted;
            }
            if (next == ' ' && !quoted) {
                break;
            }
            reader.skip();
        }
        const std::string text =
            reader.string().substr(static_cast<std::size_t>(start), static_cast<std::size_t>(reader.cursor() - start));
        if (text.empty()) {
            return std::unexpected{reader.syntaxError("Expected an NBT path")};
        }
        return ArgumentValue::of(text);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kNbtPath}; }
};

} // namespace

bool BlockInput::isAir() const { return stateId == reg::blocks::air().stateId; }

BlockPredicateInput::BlockPredicateInput(std::optional<Identifier> block, std::optional<Identifier> tag,
                                         std::map<std::string, std::string> properties, TagMembers tagMembers)
    : block_(std::move(block)), tag_(std::move(tag)), properties_(std::move(properties)),
      tagMembers_(std::move(tagMembers)) {}

bool BlockPredicateInput::matches(std::int32_t stateId) const {
    const std::optional<reg::BlockState> state = reg::blocks::byStateId(stateId);
    if (!state.has_value()) {
        return false;
    }
    if (block_.has_value() && state->block != *block_) {
        return false;
    }
    if (tag_.has_value()) {
        const std::optional<std::set<Identifier>> members = tagMembers_ ? tagMembers_(*tag_) : std::nullopt;
        if (!members.has_value() || !members->contains(state->block)) {
            return false;
        }
    }
    return std::ranges::all_of(properties_, [&state](const auto& wanted) {
        const auto found = state->properties.find(wanted.first);
        return found != state->properties.end() && found->second == wanted.second;
    });
}

ItemPredicateInput::ItemPredicateInput(std::optional<Identifier> item, std::optional<Identifier> tag,
                                       TagMembers tagMembers)
    : item_(std::move(item)), tag_(std::move(tag)), tagMembers_(std::move(tagMembers)) {}

bool ItemPredicateInput::matches(std::string_view itemId) const {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(itemId);
    if (!id.has_value()) {
        return false;
    }
    if (item_.has_value()) {
        return *id == *item_;
    }
    if (!tag_.has_value()) {
        return false;
    }
    const std::optional<std::set<Identifier>> members = tagMembers_ ? tagMembers_(*tag_) : std::nullopt;
    return members.has_value() && members->contains(*id);
}

std::shared_ptr<const ArgumentType> blockStateArgument() {
    static const auto type = std::make_shared<const BlockStateArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> blockPredicateArgument() {
    static const auto type = std::make_shared<const BlockPredicateArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> itemPredicateArgument() {
    static const auto type = std::make_shared<const ItemPredicateArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> itemArgument() {
    static const auto type = std::make_shared<const ItemArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> scoreHolderArgument() {
    static const auto type = std::make_shared<const ScoreHolderArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> operationArgument() {
    static const auto type = std::make_shared<const OperationArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> nbtCompoundArgument() {
    static const auto type = std::make_shared<const NbtCompoundArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> nbtTagArgument() {
    static const auto type = std::make_shared<const NbtTagArgumentType>();
    return type;
}

std::shared_ptr<const ArgumentType> nbtPathArgument() {
    static const auto type = std::make_shared<const NbtPathArgumentType>();
    return type;
}

} // namespace basalt::server::runtime::command
