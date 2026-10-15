#include "SelectorParser.hpp"

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::runtime::command::selectorParser {
std::optional<CommandSyntaxError> SelectorParser::readOption(const std::string& key, std::int32_t start) {
    const auto readAnchor = [this, start, &key](std::optional<double>& into) -> std::optional<CommandSyntaxError> {
        if (into.has_value()) {
            return inapplicable(start, key);
        }
        parts_.worldLimited = true;
        const Parsed<double> value = reader_->readDouble();
        if (!value.has_value()) {
            return value.error();
        }
        into = *value;
        return std::nullopt;
    };
    const auto readRotationInto =
        [this, start, &key](std::optional<SelectorRange<double>>& into) -> std::optional<CommandSyntaxError> {
        if (into.has_value()) {
            return inapplicable(start, key);
        }
        const Parsed<SelectorRange<double>> range = readRange<double>(false);
        if (!range.has_value()) {
            return range.error();
        }
        into = *range;
        return std::nullopt;
    };

    if (key == "name") {
        const bool inverted = shouldInvert();
        const Parsed<std::string> name = reader_->readString();
        if (!name.has_value()) {
            return name.error();
        }
        if (!nameOption_.canParse(inverted)) {
            return inapplicable(start, "name");
        }
        nameOption_.mark(inverted);
        parts_.predicates.emplace_back(
            [wanted = *name, inverted](const CommandTarget& target) { return (target.name() == wanted) != inverted; });
        return std::nullopt;
    }
    if (key == "distance") {
        if (parts_.distance.has_value()) {
            return inapplicable(start, "distance");
        }
        const Parsed<SelectorRange<double>> range = readRange<double>(true);
        if (!range.has_value()) {
            return range.error();
        }
        if (range->min.value_or(0.0) < 0.0 || range->max.value_or(0.0) < 0.0) {
            reader_->setCursor(start);
            return error("argument.entity.options.distance.negative");
        }
        parts_.distance = *range;
        parts_.worldLimited = true;
        return std::nullopt;
    }
    if (key == "level") {
        if (level_.has_value()) {
            return inapplicable(start, "level");
        }
        const Parsed<SelectorRange<std::int32_t>> range = readRange<std::int32_t>(true);
        if (!range.has_value()) {
            return range.error();
        }
        if (range->min.value_or(0) < 0 || range->max.value_or(0) < 0) {
            reader_->setCursor(start);
            return error("argument.entity.options.level.negative");
        }
        level_ = *range;
        parts_.includesEntities = false;
        return std::nullopt;
    }
    if (key == "x") {
        return readAnchor(parts_.anchorX);
    }
    if (key == "y") {
        return readAnchor(parts_.anchorY);
    }
    if (key == "z") {
        return readAnchor(parts_.anchorZ);
    }
    if (key == "dx") {
        return readAnchor(parts_.deltaX);
    }
    if (key == "dy") {
        return readAnchor(parts_.deltaY);
    }
    if (key == "dz") {
        return readAnchor(parts_.deltaZ);
    }
    if (key == "x_rotation") {
        return readRotationInto(rotX_);
    }
    if (key == "y_rotation") {
        return readRotationInto(rotY_);
    }
    if (key == "limit") {
        if (parts_.currentEntity || limitSet_) {
            return inapplicable(start, "limit");
        }
        const std::int32_t cursor = reader_->cursor();
        const Parsed<std::int32_t> count = reader_->readInt();
        if (!count.has_value()) {
            return count.error();
        }
        if (*count < 1) {
            reader_->setCursor(cursor);
            return error("argument.entity.options.limit.toosmall");
        }
        parts_.maxResults = *count;
        limitSet_ = true;
        return std::nullopt;
    }
    if (key == "sort") {
        if (parts_.currentEntity || sortSet_) {
            return inapplicable(start, "sort");
        }
        const std::int32_t cursor = reader_->cursor();
        const std::string name = reader_->readUnquotedString();
        if (name == "nearest") {
            parts_.order = SelectorOrder::Nearest;
        } else if (name == "furthest") {
            parts_.order = SelectorOrder::Furthest;
        } else if (name == "random") {
            parts_.order = SelectorOrder::Random;
        } else if (name == "arbitrary") {
            parts_.order = SelectorOrder::Arbitrary;
        } else {
            reader_->setCursor(cursor);
            return error("argument.entity.options.sort.irreversible", name);
        }
        sortSet_ = true;
        return std::nullopt;
    }
    if (key == "gamemode") {
        const bool inverted = shouldInvert();
        if (!gamemodeOption_.canParse(inverted)) {
            return inapplicable(start, "gamemode");
        }
        const std::int32_t cursor = reader_->cursor();
        const std::string name = reader_->readUnquotedString();
        static constexpr std::array<std::pair<std::string_view, Gamemode>, 4> kModes{
            {{"survival", Gamemode::Survival},
             {"creative", Gamemode::Creative},
             {"adventure", Gamemode::Adventure},
             {"spectator", Gamemode::Spectator}}};
        std::string lowered;
        for (const char letter : name) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
        }
        const auto found = std::ranges::find(kModes, lowered, &std::pair<std::string_view, Gamemode>::first);
        if (found == kModes.end()) {
            reader_->setCursor(cursor);
            return error("argument.entity.options.mode.invalid", name);
        }
        parts_.includesEntities = false;
        gamemodeOption_.mark(inverted);
        parts_.predicates.emplace_back([expected = found->second, inverted](const CommandTarget& target) {
            const player::PlayerSession* session = target.player();
            return (session != nullptr && session->gamemode == expected) != inverted;
        });
        return std::nullopt;
    }
    if (key == "team") {
        const bool inverted = shouldInvert();
        const std::string expected = reader_->readUnquotedString();
        if (!teamOption_.canParse(inverted)) {
            return inapplicable(start, "team");
        }
        teamOption_.mark(inverted);
        const scoreboard::Teams* teams = environment_->options().teams;
        parts_.predicates.emplace_back([teams, expected, inverted](const CommandTarget& target) {
            const scoreboard::Team* team = teams == nullptr ? nullptr : teams->teamOf(target.scoreboardName());
            return ((team == nullptr ? std::string{} : team->name) == expected) != inverted;
        });
        return std::nullopt;
    }
    if (key == "type") {
        const bool inverted = shouldInvert();
        if (reader_->canRead() && reader_->peek() == '#') {
            reader_->skip();
            const Parsed<Identifier> tag = readIdentifier(start);
            if (!tag.has_value()) {
                return tag.error();
            }
            if (!typeOption_.canParseTag(*tag)) {
                return inapplicable(start, "type");
            }
            typeOption_.markTag(*tag);
            const CommandEnvironment::TagMembers& members = environment_->options().entityTagMembers;
            std::set<Identifier> held;
            if (members) {
                if (const std::optional<std::set<Identifier>> found = members(*tag); found.has_value()) {
                    held = *found;
                }
            }
            parts_.predicates.emplace_back([held = std::move(held), inverted](const CommandTarget& target) {
                return held.contains(target.type()) != inverted;
            });
            return std::nullopt;
        }
        if (!typeOption_.canParse(inverted)) {
            return inapplicable(start, "type");
        }
        const Parsed<Identifier> id = readIdentifier(start);
        if (!id.has_value()) {
            return id.error();
        }
        if (server::registry::entityTypes::of(*id) == nullptr) {
            reader_->setCursor(start);
            return error("argument.entity.options.type.invalid", id->toString());
        }
        if (*id == Identifier::vanilla("player") && !inverted) {
            parts_.includesEntities = false;
        }
        if (!inverted) {
            parts_.typeLimit = *id;
        }
        typeOption_.mark(inverted);
        parts_.predicates.emplace_back(
            [wanted = *id, inverted](const CommandTarget& target) { return (target.type() == wanted) != inverted; });
        return std::nullopt;
    }
    if (key == "tag") {
        const bool inverted = shouldInvert();
        const std::string tag = reader_->readUnquotedString();
        parts_.predicates.emplace_back([tag, inverted](const CommandTarget& target) {
            const std::set<std::string> tags = target.tags();
            const bool has = tag.empty() ? tags.empty() : tags.contains(tag);
            return has != inverted;
        });
        return std::nullopt;
    }
    if (key == "nbt") {
        const bool inverted = shouldInvert();
        const std::int32_t at = reader_->cursor();
        const std::expected<wire::nbt::SnbtValue, wire::nbt::NbtError> parsed =
            wire::nbt::snbt::parseFrom(reader_->string(), static_cast<std::size_t>(at));
        if (!parsed.has_value()) {
            reader_->setCursor(at);
            return reader_->syntaxError("Invalid NBT");
        }
        reader_->setCursor(static_cast<std::int32_t>(parsed->position));
        const NbtCompound* expected = parsed->tag.get<NbtCompound>();
        if (expected == nullptr) {
            return reader_->syntaxError("Expected a compound");
        }
        parts_.predicates.emplace_back([wanted = *expected, inverted](const CommandTarget& target) {
            return containsNbt(entityNbtView::of(target), wanted) != inverted;
        });
        return std::nullopt;
    }
    if (key == "scores") {
        if (scoresSet_) {
            return inapplicable(start, "scores");
        }
        std::map<std::string, SelectorRange<std::int32_t>, std::less<>> expected;
        if (const Parsed<void> open = reader_->expect('{'); !open.has_value()) {
            return open.error();
        }
        reader_->skipWhitespace();
        while (reader_->canRead() && reader_->peek() != '}') {
            reader_->skipWhitespace();
            const std::string objective = reader_->readUnquotedString();
            reader_->skipWhitespace();
            if (const Parsed<void> equals = reader_->expect('='); !equals.has_value()) {
                return equals.error();
            }
            reader_->skipWhitespace();
            const Parsed<SelectorRange<std::int32_t>> range = readRange<std::int32_t>(true);
            if (!range.has_value()) {
                return range.error();
            }
            expected.insert_or_assign(objective, *range);
            reader_->skipWhitespace();
            if (reader_->canRead() && reader_->peek() == ',') {
                reader_->skip();
            }
        }
        if (const Parsed<void> close = reader_->expect('}'); !close.has_value()) {
            return close.error();
        }
        scoresSet_ = true;
        if (expected.empty()) {
            return std::nullopt;
        }
        const scoreboard::Scoreboard* board = environment_->options().scoreboard;
        parts_.predicates.emplace_back([board, expected = std::move(expected)](const CommandTarget& target) {
            return std::ranges::all_of(expected, [board, &target](const auto& entry) {
                const std::optional<std::int32_t> value =
                    board == nullptr ? std::nullopt : board->score(target.scoreboardName(), entry.first);
                return value.has_value() && entry.second.matches(*value);
            });
        });
        return std::nullopt;
    }
    if (key == "advancements") {
        if (advancementsSet_) {
            return inapplicable(start, "advancements");
        }
        std::map<Identifier, std::optional<std::map<std::string, bool, std::less<>>>> expected;
        std::map<Identifier, bool> whole;
        if (const Parsed<void> open = reader_->expect('{'); !open.has_value()) {
            return open.error();
        }
        reader_->skipWhitespace();
        while (reader_->canRead() && reader_->peek() != '}') {
            reader_->skipWhitespace();
            const Parsed<Identifier> id = readIdentifier(reader_->cursor());
            if (!id.has_value()) {
                return id.error();
            }
            reader_->skipWhitespace();
            if (const Parsed<void> equals = reader_->expect('='); !equals.has_value()) {
                return equals.error();
            }
            reader_->skipWhitespace();
            if (reader_->canRead() && reader_->peek() == '{') {
                reader_->skip();
                std::map<std::string, bool, std::less<>> criteria;
                reader_->skipWhitespace();
                while (reader_->canRead() && reader_->peek() != '}') {
                    reader_->skipWhitespace();
                    const std::string criterion = reader_->readUnquotedString();
                    reader_->skipWhitespace();
                    if (const Parsed<void> equals = reader_->expect('='); !equals.has_value()) {
                        return equals.error();
                    }
                    reader_->skipWhitespace();
                    const Parsed<bool> done = readBoolean();
                    if (!done.has_value()) {
                        return done.error();
                    }
                    criteria.insert_or_assign(criterion, *done);
                    reader_->skipWhitespace();
                    if (reader_->canRead() && reader_->peek() == ',') {
                        reader_->skip();
                    }
                }
                reader_->skipWhitespace();
                if (const Parsed<void> close = reader_->expect('}'); !close.has_value()) {
                    return close.error();
                }
                expected.insert_or_assign(*id, std::move(criteria));
            } else {
                const Parsed<bool> done = readBoolean();
                if (!done.has_value()) {
                    return done.error();
                }
                whole.insert_or_assign(*id, *done);
                expected.insert_or_assign(*id, std::nullopt);
            }
            reader_->skipWhitespace();
            if (reader_->canRead() && reader_->peek() == ',') {
                reader_->skip();
            }
        }
        if (const Parsed<void> close = reader_->expect('}'); !close.has_value()) {
            return close.error();
        }
        advancementsSet_ = true;
        if (expected.empty()) {
            return std::nullopt;
        }
        parts_.includesEntities = false;
        const world::RealAdvancements* advancements = environment_->options().advancements;
        world::PlayerProgress* progress = environment_->options().progress;
        parts_.predicates.emplace_back([advancements, progress, expected = std::move(expected),
                                        whole = std::move(whole)](const CommandTarget& target) {
            if (target.player() == nullptr) {
                return false;
            }
            return std::ranges::all_of(expected, [&](const auto& entry) {
                const server::world::advancement::Advancement* advancement =
                    advancements == nullptr ? nullptr : advancements->find(entry.first);
                if (advancement == nullptr) {
                    return false;
                }
                const std::set<std::string, std::less<>> granted =
                    progress == nullptr ? std::set<std::string, std::less<>>{}
                                        : progress->grantedCriteria(target.uuid(), entry.first);
                if (entry.second.has_value()) {
                    return std::ranges::all_of(*entry.second, [&granted](const auto& criterion) {
                        return granted.contains(criterion.first) == criterion.second;
                    });
                }
                const auto wanted = whole.find(entry.first);
                return advancement->isComplete(granted) == (wanted != whole.end() && wanted->second);
            });
        });
        return std::nullopt;
    }
    if (key == "predicate") {
        // parity: an id no pack declares is false, negated or not.
        const bool inverted = shouldInvert();
        const Parsed<Identifier> id = readIdentifier(reader_->cursor());
        if (!id.has_value()) {
            return id.error();
        }
        parts_.predicates.emplace_back([environment = environment_, id = *id, inverted](const CommandTarget& target) {
            if (!environment->options().predicate) {
                return false;
            }
            const auto& worlds = environment->options().worlds;
            const auto found = worlds.find(target.worldName());
            const std::optional<bool> passed = environment->options().predicate(
                wire::common::JsonValue::of(id.toString()), &target, found == worlds.end() ? nullptr : found->second,
                {target.x(), target.y(), target.z()});
            return passed.has_value() && *passed != inverted;
        });
        return std::nullopt;
    }
    reader_->setCursor(start);
    return error("argument.entity.options.unknown", key);
}

} // namespace basalt::server::runtime::command::selectorParser
