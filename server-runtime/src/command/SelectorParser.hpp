#pragma once

#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::command::selectorParser {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::protocol::play::Gamemode;
using wire::text::Component;

/// The bracket syntax of a selector, split across two files: the walk and its helpers here, the
/// option table beside them. One class rather than two because the options fill in the same parts.
constexpr std::size_t kUuidLength = 36;
constexpr std::size_t kUuidDashes = 4;
constexpr std::size_t kMaxPlayerName = 16;
constexpr double kFullTurn = 360.0;
constexpr double kHalfTurn = 180.0;
constexpr double kDefaultRotationMax = 359.0;

/// Whether the actual tag carries everything the expected one asks for.
/// parity: `NbtUtils.compareNbt` with lists ordered.
[[nodiscard]] inline bool containsNbt(const NbtCompound& actual, const NbtCompound& expected) {
    for (std::size_t index = 0; index < expected.names().size(); index++) {
        const NbtTag* present = actual.find(expected.names()[index]);
        if (present == nullptr) {
            return false;
        }
        const NbtTag& wanted = expected.values()[index];
        const NbtCompound* wantedCompound = wanted.get<NbtCompound>();
        const NbtCompound* presentCompound = present->get<NbtCompound>();
        if (wantedCompound != nullptr && presentCompound != nullptr) {
            if (!containsNbt(*presentCompound, *wantedCompound)) {
                return false;
            }
        } else if (!(wanted == *present)) {
            return false;
        }
    }
    return true;
}

/// How often an option may appear, and with which sign. parity: `InvertableSetOptionState`.
///
/// One positive value settles the question, so nothing else may follow it; negated values only
/// remove candidates, so any number of them is fine.
class InvertableOption {
public:
    [[nodiscard]] bool canParse(bool inverted) const { return inverted ? !single_ : !single_ && !multiple_; }

    [[nodiscard]] bool canParseTag(const Identifier& tag) const { return !single_ && !tags_.contains(tag); }

    void mark(bool inverted) {
        if (inverted) {
            multiple_ = true;
        } else {
            single_ = true;
        }
    }

    void markTag(const Identifier& tag) {
        multiple_ = true;
        tags_.insert(tag);
    }

private:
    bool single_ = false;
    bool multiple_ = false;
    std::set<Identifier> tags_;
};

/// The bracket syntax, one option at a time. parity: `EntitySelectorParser.parseOptions` and
/// `EntitySelectorOptions`, error keys included: the client shows them, so they are specification.
///
/// The "can this option appear again" rules are vanilla's own and are not cosmetic:
/// `type=zombie,type=creeper` can never match, so it is refused, while `type=!zombie,type=!creeper`
/// is fine. Same for `name`, `gamemode` and `team`.
class SelectorParser {
public:
    /// The source is not read here: every option that depends on who is asking is answered when
    /// the selector runs, not when it is parsed.
    SelectorParser(StringReader& reader, const CommandEnvironment& environment)
        : reader_(&reader), environment_(&environment) {}

    [[nodiscard]] Parsed<EntitySelector> parse() {
        if (reader_->canRead() && reader_->peek() == '@') {
            reader_->skip();
            if (const std::optional<CommandSyntaxError> failure = parseSelector(); failure.has_value()) {
                return std::unexpected{*failure};
            }
        } else if (const std::optional<CommandSyntaxError> failure = parseNameOrUuid(); failure.has_value()) {
            return std::unexpected{*failure};
        }
        finalizePredicates();
        return EntitySelector{std::move(parts_)};
    }

private:
    [[nodiscard]] CommandSyntaxError error(const std::string& key,
                                           std::optional<std::string> argument = std::nullopt) const {
        return CommandSyntaxError{.component = argument.has_value()
                                                   ? Component::translatable(key, {Component::text(*argument)})
                                                   : Component::translatable(key),
                                  .input = reader_->string(),
                                  .cursor = reader_->cursor()};
    }

    [[nodiscard]] CommandSyntaxError inapplicable(std::int32_t start, const std::string& key) {
        reader_->setCursor(start);
        return error("argument.entity.options.inapplicable", key);
    }

    [[nodiscard]] std::optional<CommandSyntaxError> parseSelector() {
        if (!reader_->canRead()) {
            return error("argument.entity.selector.missing");
        }
        const std::int32_t start = reader_->cursor();
        const char letter = reader_->read();
        bool livingOnly = false;
        switch (letter) {
        case 'a':
            parts_.maxResults = std::numeric_limits<std::int32_t>::max();
            parts_.typeLimit = Identifier::vanilla("player");
            break;
        case 'e':
            parts_.maxResults = std::numeric_limits<std::int32_t>::max();
            parts_.includesEntities = true;
            livingOnly = true;
            break;
        case 'n':
            parts_.maxResults = 1;
            parts_.includesEntities = true;
            parts_.order = SelectorOrder::Nearest;
            livingOnly = true;
            break;
        case 'p':
            parts_.maxResults = 1;
            parts_.order = SelectorOrder::Nearest;
            parts_.typeLimit = Identifier::vanilla("player");
            break;
        case 'r':
            parts_.maxResults = 1;
            parts_.order = SelectorOrder::Random;
            parts_.typeLimit = Identifier::vanilla("player");
            break;
        case 's':
            parts_.maxResults = 1;
            parts_.includesEntities = true;
            parts_.currentEntity = true;
            break;
        default:
            reader_->setCursor(start);
            return error("argument.entity.selector.unknown", std::string{"@"} + letter);
        }
        if (livingOnly) {
            parts_.predicates.emplace_back([](const CommandTarget& target) { return target.alive(); });
        }
        if (reader_->canRead() && reader_->peek() == '[') {
            reader_->skip();
            return parseOptions();
        }
        return std::nullopt;
    }

    /// parity: a bare word is a player name, unless it parses as a UUID, and never an entity's
    /// custom name.
    [[nodiscard]] std::optional<CommandSyntaxError> parseNameOrUuid() {
        const std::int32_t start = reader_->cursor();
        const Parsed<std::string> read = reader_->readString();
        if (!read.has_value()) {
            return read.error();
        }
        const std::string& name = *read;
        if (name.size() == kUuidLength && static_cast<std::size_t>(std::ranges::count(name, '-')) == kUuidDashes) {
            parts_.entityUuid = name;
            parts_.includesEntities = true;
        } else {
            if (name.empty() || name.size() > kMaxPlayerName) {
                reader_->setCursor(start);
                return error("argument.entity.invalid");
            }
            parts_.playerName = name;
        }
        parts_.maxResults = 1;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<CommandSyntaxError> parseOptions() {
        reader_->skipWhitespace();
        while (reader_->canRead() && reader_->peek() != ']') {
            reader_->skipWhitespace();
            const std::int32_t start = reader_->cursor();
            const Parsed<std::string> key = reader_->readString();
            if (!key.has_value()) {
                return key.error();
            }
            reader_->skipWhitespace();
            if (!reader_->canRead() || reader_->peek() != '=') {
                reader_->setCursor(start);
                return error("argument.entity.options.valueless", *key);
            }
            reader_->skip();
            reader_->skipWhitespace();
            if (const std::optional<CommandSyntaxError> failure = readOption(*key, start); failure.has_value()) {
                return failure;
            }
            reader_->skipWhitespace();
            if (reader_->canRead()) {
                if (reader_->peek() != ',') {
                    if (reader_->peek() != ']') {
                        return error("argument.entity.options.unterminated");
                    }
                    break;
                }
                reader_->skip();
            }
        }
        if (!reader_->canRead()) {
            return error("argument.entity.options.unterminated");
        }
        reader_->skip();
        return std::nullopt;
    }

    [[nodiscard]] std::optional<CommandSyntaxError> readOption(const std::string& key, std::int32_t start);

    [[nodiscard]] bool shouldInvert() {
        reader_->skipWhitespace();
        if (reader_->canRead() && reader_->peek() == '!') {
            reader_->skip();
            reader_->skipWhitespace();
            return true;
        }
        return false;
    }

    [[nodiscard]] Parsed<bool> readBoolean() {
        const std::string word = reader_->readUnquotedString();
        if (word == "true") {
            return true;
        }
        if (word == "false") {
            return false;
        }
        return std::unexpected{reader_->syntaxError("Expected true or false, got '" + word + "'")};
    }

    [[nodiscard]] Parsed<Identifier> readIdentifier(std::int32_t start) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed =
            Identifier::parse(reader_->readIdentifier());
        if (!parsed.has_value()) {
            reader_->setCursor(start);
            return std::unexpected{reader_->syntaxError("Invalid identifier")};
        }
        return *parsed;
    }

    /// Digits, a sign and a decimal point, but never the `.` that opens the `..` separator.
    [[nodiscard]] std::string readRangeNumber() {
        const std::int32_t start = reader_->cursor();
        while (reader_->canRead()) {
            const char next = reader_->peek();
            const bool allowed = (next >= '0' && next <= '9') || next == '-' ||
                                 (next == '.' && (!reader_->canRead(2) || reader_->peek(1) != '.'));
            if (!allowed) {
                break;
            }
            reader_->skip();
        }
        return reader_->string().substr(static_cast<std::size_t>(start),
                                        static_cast<std::size_t>(reader_->cursor() - start));
    }

    /// parity: `MinMaxBounds.Bounds.fromReader`, including `..` as the only separator and both ends
    /// optional. The degrees form is the one that does **not** reject a swapped range:
    /// `y_rotation=160..-160` is the arc through south, not a mistake.
    template <typename T>
    [[nodiscard]] Parsed<SelectorRange<T>> readRange(bool rejectSwapped) {
        if (!reader_->canRead()) {
            return std::unexpected{error("argument.range.empty")};
        }
        const std::int32_t start = reader_->cursor();
        const std::string minText = readRangeNumber();
        std::optional<T> min;
        if (!minText.empty()) {
            const std::optional<T> value = numberOf<T>(minText);
            if (!value.has_value()) {
                reader_->setCursor(start);
                return std::unexpected{reader_->syntaxError("Invalid number '" + minText + "'")};
            }
            min = value;
        }
        std::optional<T> max = min;
        if (reader_->canRead(2) && reader_->peek() == '.' && reader_->peek(1) == '.') {
            reader_->skip();
            reader_->skip();
            const std::string maxText = readRangeNumber();
            max.reset();
            if (!maxText.empty()) {
                const std::optional<T> value = numberOf<T>(maxText);
                if (!value.has_value()) {
                    reader_->setCursor(start);
                    return std::unexpected{reader_->syntaxError("Invalid number '" + maxText + "'")};
                }
                max = value;
            }
        }
        if (!min.has_value() && !max.has_value()) {
            reader_->setCursor(start);
            return std::unexpected{error("argument.range.empty")};
        }
        if (rejectSwapped && min.has_value() && max.has_value() && *min > *max) {
            reader_->setCursor(start);
            return std::unexpected{error("argument.range.swapped")};
        }
        return SelectorRange<T>{.min = min, .max = max};
    }

    template <typename T>
    [[nodiscard]] static std::optional<T> numberOf(const std::string& text) {
        T value{};
        const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
            return std::nullopt;
        }
        return value;
    }

    /// parity: a wrapped range, so `y_rotation=170..-170` is the arc that crosses south.
    [[nodiscard]] static bool rotationMatches(const SelectorRange<double>& range, float value) {
        const double min = wrapDegrees(range.min.value_or(0.0));
        const double max = wrapDegrees(range.max.value_or(kDefaultRotationMax));
        const double rotation = wrapDegrees(static_cast<double>(value));
        if (min > max) {
            return rotation >= min || rotation <= max;
        }
        return rotation >= min && rotation <= max;
    }

    [[nodiscard]] static double wrapDegrees(double value) {
        double wrapped = std::fmod(value, kFullTurn);
        if (wrapped >= kHalfTurn) {
            wrapped -= kFullTurn;
        }
        if (wrapped < -kHalfTurn) {
            wrapped += kFullTurn;
        }
        return wrapped;
    }

    void finalizePredicates() {
        if (rotX_.has_value()) {
            const SelectorRange<double> range = *rotX_;
            parts_.predicates.emplace_back(
                [range](const CommandTarget& target) { return rotationMatches(range, target.pitch()); });
        }
        if (rotY_.has_value()) {
            const SelectorRange<double> range = *rotY_;
            parts_.predicates.emplace_back(
                [range](const CommandTarget& target) { return rotationMatches(range, target.yaw()); });
        }
        if (level_.has_value()) {
            const SelectorRange<std::int32_t> range = *level_;
            parts_.predicates.emplace_back([range](const CommandTarget& target) {
                const player::PlayerSession* session = target.player();
                return session != nullptr && range.matches(session->experience.level());
            });
        }
    }

    StringReader* reader_;
    const CommandEnvironment* environment_;
    EntitySelector::Parts parts_;
    std::optional<SelectorRange<std::int32_t>> level_;
    std::optional<SelectorRange<double>> rotX_;
    std::optional<SelectorRange<double>> rotY_;
    InvertableOption nameOption_;
    InvertableOption gamemodeOption_;
    InvertableOption teamOption_;
    InvertableOption typeOption_;
    bool limitSet_ = false;
    bool sortSet_ = false;
    bool scoresSet_ = false;
    bool advancementsSet_ = false;
};

} // namespace basalt::server::runtime::command::selectorParser
