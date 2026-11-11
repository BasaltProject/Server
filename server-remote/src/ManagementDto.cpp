#include "ManagementDto.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>

namespace basalt::server::remote::managementDto {
namespace {

constexpr std::int64_t kSecondsPerDay = 86400;

/// Days since 1970-01-01 of a proleptic Gregorian date.
[[nodiscard]] std::int64_t daysFromCivil(std::int64_t year, std::int64_t month, std::int64_t day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yearOfEra = year - (era * 400);
    const std::int64_t dayOfYear = ((153 * (month + (month > 2 ? -3 : 9))) + 2) / 5 + day - 1;
    const std::int64_t dayOfEra = (yearOfEra * 365) + (yearOfEra / 4) - (yearOfEra / 100) + dayOfYear;
    return (era * 146097) + dayOfEra - 719468;
}

void civilFromDays(std::int64_t days, std::int64_t& year, std::int64_t& month, std::int64_t& day) {
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const std::int64_t dayOfEra = days - (era * 146097);
    const std::int64_t yearOfEra = (dayOfEra - (dayOfEra / 1460) + (dayOfEra / 36524) - (dayOfEra / 146096)) / 365;
    year = yearOfEra + (era * 400);
    const std::int64_t dayOfYear = dayOfEra - ((365 * yearOfEra) + (yearOfEra / 4) - (yearOfEra / 100));
    const std::int64_t shifted = ((5 * dayOfYear) + 2) / 153;
    day = dayOfYear - (((153 * shifted) + 2) / 5) + 1;
    month = shifted < 10 ? shifted + 3 : shifted - 9;
    year += month <= 2 ? 1 : 0;
}

[[nodiscard]] bool digits(std::string_view text, std::size_t at, std::size_t count, std::int64_t& out) {
    if (at + count > text.size()) {
        return false;
    }
    out = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const char c = text[at + i];
        if (c < '0' || c > '9') {
            return false;
        }
        out = (out * 10) + (c - '0');
    }
    return true;
}

[[nodiscard]] bool isUuid(std::string_view text) {
    if (text.size() != 36) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash ? text[i] != '-' : std::isxdigit(static_cast<unsigned char>(text[i])) == 0) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const JsonValue* field(const JsonObject& object, std::string_view key) { return object.find(key); }

[[nodiscard]] Parsed<std::optional<std::string>> optionalText(const JsonObject& object, std::string_view key) {
    const JsonValue* found = field(object, key);
    if (found == nullptr) {
        return std::optional<std::string>{};
    }
    const auto* text = found->get<std::string>();
    if (text == nullptr) {
        return std::unexpected{"Not a string: " + std::string{key}};
    }
    return std::optional<std::string>{*text};
}

[[nodiscard]] Parsed<std::optional<std::int64_t>> optionalInstant(const JsonObject& object, std::string_view key) {
    const auto text = optionalText(object, key);
    if (!text.has_value()) {
        return std::unexpected{text.error()};
    }
    if (!text->has_value()) {
        return std::optional<std::int64_t>{};
    }
    const std::optional<std::int64_t> parsed = parseInstant(**text);
    if (!parsed.has_value()) {
        return std::unexpected{"Failed to parse date: " + **text};
    }
    return std::optional<std::int64_t>{*parsed};
}

[[nodiscard]] const JsonObject* asObject(const JsonValue& value) { return value.get<JsonObject>(); }

void put(JsonObject& object, std::string_view key, const std::optional<std::string>& text) {
    if (text.has_value()) {
        object.set(key, JsonValue::of(*text));
    }
}

void putInstant(JsonObject& object, const std::optional<std::int64_t>& expires) {
    if (expires.has_value()) {
        object.set("expires", JsonValue::of(formatInstant(*expires)));
    }
}

} // namespace

std::string formatInstant(const std::int64_t epochSeconds) {
    std::int64_t days = epochSeconds / kSecondsPerDay;
    std::int64_t rest = epochSeconds % kSecondsPerDay;
    if (rest < 0) {
        rest += kSecondsPerDay;
        days -= 1;
    }
    std::int64_t year = 0;
    std::int64_t month = 0;
    std::int64_t day = 0;
    civilFromDays(days, year, month, day);
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%04lld-%02lld-%02lldT%02lld:%02lld:%02lldZ", static_cast<long long>(year),
                  static_cast<long long>(month), static_cast<long long>(day), static_cast<long long>(rest / 3600),
                  static_cast<long long>((rest / 60) % 60), static_cast<long long>(rest % 60));
    return buffer;
}

std::optional<std::int64_t> parseInstant(const std::string_view text) {
    std::int64_t year = 0;
    std::int64_t month = 0;
    std::int64_t day = 0;
    std::int64_t hour = 0;
    std::int64_t minute = 0;
    std::int64_t second = 0;
    if (text.size() < 20 || !digits(text, 0, 4, year) || text[4] != '-' || !digits(text, 5, 2, month) ||
        text[7] != '-' || !digits(text, 8, 2, day) || (text[10] != 'T' && text[10] != 't') ||
        !digits(text, 11, 2, hour) || text[13] != ':' || !digits(text, 14, 2, minute) || text[16] != ':' ||
        !digits(text, 17, 2, second)) {
        return std::nullopt;
    }
    std::size_t at = 19;
    if (text[at] == '.') {
        ++at;
        const std::size_t start = at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            ++at;
        }
        if (at == start || at - start > 9) {
            return std::nullopt;
        }
    }
    if (at + 1 != text.size() || (text[at] != 'Z' && text[at] != 'z')) {
        return std::nullopt;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) {
        return std::nullopt;
    }
    return (daysFromCivil(year, month, day) * kSecondsPerDay) + (hour * 3600) + (minute * 60) + second;
}

bool looksLikeAddress(const std::string_view text) {
    if (text.empty()) {
        return false;
    }
    if (text.find(':') != std::string_view::npos) {
        return std::ranges::all_of(
            text, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0 || c == ':' || c == '.'; });
    }
    std::int32_t parts = 0;
    std::size_t at = 0;
    while (at <= text.size()) {
        const std::size_t dot = text.find('.', at);
        const std::string_view part = text.substr(at, dot == std::string_view::npos ? text.npos : dot - at);
        std::int32_t value = 0;
        const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
        if (part.empty() || part.size() > 3 || error != std::errc{} || end != part.data() + part.size() ||
            value > 255) {
            return false;
        }
        ++parts;
        if (dot == std::string_view::npos) {
            break;
        }
        at = dot + 1;
    }
    return parts == 4;
}

JsonValue encodePlayer(const ManagedPlayer& player) {
    JsonObject object;
    put(object, "id", player.id);
    put(object, "name", player.name);
    return JsonValue::ofObject(std::move(object));
}

JsonValue encodeUserBan(const ManagedUserBan& ban) {
    JsonObject object;
    object.set("player", encodePlayer(ban.player));
    put(object, "reason", ban.reason);
    put(object, "source", ban.source);
    putInstant(object, ban.expiresEpochSeconds);
    return JsonValue::ofObject(std::move(object));
}

JsonValue encodeIpBan(const ManagedIpBan& ban) {
    JsonObject object;
    object.set("ip", JsonValue::of(ban.ip));
    put(object, "reason", ban.reason);
    put(object, "source", ban.source);
    putInstant(object, ban.expiresEpochSeconds);
    return JsonValue::ofObject(std::move(object));
}

JsonValue encodeOperator(const ManagedOperator& entry) {
    JsonObject object;
    object.set("player", encodePlayer(entry.player));
    if (entry.permissionLevel.has_value()) {
        object.set("permissionLevel", JsonValue::of(static_cast<std::int64_t>(*entry.permissionLevel)));
    }
    if (entry.bypassesPlayerLimit.has_value()) {
        object.set("bypassesPlayerLimit", JsonValue::of(*entry.bypassesPlayerLimit));
    }
    return JsonValue::ofObject(std::move(object));
}

JsonValue encodeTypedRule(const ManagedGameRule& rule) {
    JsonObject object;
    object.set("key", JsonValue::of(rule.key));
    object.set("type", JsonValue::of(std::string{rule.boolean ? "boolean" : "integer"}));
    object.set("value", rule.boolean ? JsonValue::of(rule.flag) : JsonValue::of(rule.number));
    return JsonValue::ofObject(std::move(object));
}

JsonValue encodeServerState(const bool started, const std::vector<ManagedPlayer>& players,
                            const std::string_view versionName, const std::int32_t protocol) {
    JsonObject object;
    object.set("started", JsonValue::of(started));
    if (!players.empty()) {
        object.set("players", encodeList<ManagedPlayer>(players, &encodePlayer));
    }
    JsonObject version;
    version.set("name", JsonValue::of(std::string{versionName}));
    version.set("protocol", JsonValue::of(static_cast<std::int64_t>(protocol)));
    object.set("version", JsonValue::ofObject(std::move(version)));
    return JsonValue::ofObject(std::move(object));
}

Parsed<std::string> parseString(const JsonValue& value) {
    const auto* text = value.get<std::string>();
    if (text == nullptr) {
        return std::unexpected{std::string{"Not a string"}};
    }
    return *text;
}

Parsed<ManagedPlayer> parsePlayer(const JsonValue& value) {
    const JsonObject* object = asObject(value);
    if (object == nullptr) {
        return std::unexpected{std::string{"Not a map: player"}};
    }
    ManagedPlayer player;
    auto id = optionalText(*object, "id");
    if (!id.has_value()) {
        return std::unexpected{id.error()};
    }
    if (id->has_value() && !isUuid(**id)) {
        return std::unexpected{"Invalid UUID string: " + **id};
    }
    player.id = std::move(*id);
    auto name = optionalText(*object, "name");
    if (!name.has_value()) {
        return std::unexpected{name.error()};
    }
    player.name = std::move(*name);
    return player;
}

Parsed<ManagedMessage> parseMessage(const JsonValue& value) {
    const JsonObject* object = asObject(value);
    if (object == nullptr) {
        return std::unexpected{std::string{"Not a map: message"}};
    }
    ManagedMessage message;
    auto literal = optionalText(*object, "literal");
    auto translatable = optionalText(*object, "translatable");
    if (!literal.has_value() || !translatable.has_value()) {
        return std::unexpected{literal.has_value() ? translatable.error() : literal.error()};
    }
    message.literal = std::move(*literal);
    message.translatable = std::move(*translatable);
    if (const JsonValue* params = field(*object, "translatableParams"); params != nullptr) {
        const std::function<Parsed<std::string>(const JsonValue&)> each = parseString;
        auto list = parseList<std::string>(*params, each);
        if (list.has_value()) {
            message.translatableParams = std::move(*list);
        }
    }
    return message;
}

Parsed<ManagedUserBan> parseUserBan(const JsonValue& value) {
    const JsonObject* object = asObject(value);
    const JsonValue* player = object == nullptr ? nullptr : field(*object, "player");
    if (player == nullptr) {
        return std::unexpected{std::string{"No key player in MapLike[user_ban]"}};
    }
    auto parsedPlayer = parsePlayer(*player);
    auto reason = optionalText(*object, "reason");
    auto source = optionalText(*object, "source");
    auto expires = optionalInstant(*object, "expires");
    if (!parsedPlayer.has_value() || !reason.has_value() || !source.has_value() || !expires.has_value()) {
        return std::unexpected{!parsedPlayer.has_value() ? parsedPlayer.error()
                               : !reason.has_value()     ? reason.error()
                               : !source.has_value()     ? source.error()
                                                         : expires.error()};
    }
    return ManagedUserBan{std::move(*parsedPlayer), std::move(*reason), std::move(*source), *expires};
}

Parsed<ManagedIpBan> parseIpBan(const JsonValue& value) {
    const JsonObject* object = asObject(value);
    const JsonValue* ip = object == nullptr ? nullptr : field(*object, "ip");
    if (ip == nullptr) {
        return std::unexpected{std::string{"No key ip in MapLike[ip_ban]"}};
    }
    auto text = parseString(*ip);
    auto reason = optionalText(*object, "reason");
    auto source = optionalText(*object, "source");
    auto expires = optionalInstant(*object, "expires");
    if (!text.has_value() || !reason.has_value() || !source.has_value() || !expires.has_value()) {
        return std::unexpected{!text.has_value()     ? text.error()
                               : !reason.has_value() ? reason.error()
                               : !source.has_value() ? source.error()
                                                     : expires.error()};
    }
    return ManagedIpBan{std::move(*text), std::move(*reason), std::move(*source), *expires};
}

Parsed<IncomingIpBan> parseIncomingIpBan(const JsonValue& value) {
    const JsonObject* object = asObject(value);
    if (object == nullptr) {
        return std::unexpected{std::string{"Not a map: incoming_ip_ban"}};
    }
    IncomingIpBan ban;
    if (const JsonValue* player = field(*object, "player"); player != nullptr) {
        auto parsed = parsePlayer(*player);
        if (!parsed.has_value()) {
            return std::unexpected{parsed.error()};
        }
        ban.player = std::move(*parsed);
    }
    auto ip = optionalText(*object, "ip");
    auto reason = optionalText(*object, "reason");
    auto source = optionalText(*object, "source");
    auto expires = optionalInstant(*object, "expires");
    if (!ip.has_value() || !reason.has_value() || !source.has_value() || !expires.has_value()) {
        return std::unexpected{!ip.has_value()       ? ip.error()
                               : !reason.has_value() ? reason.error()
                               : !source.has_value() ? source.error()
                                                     : expires.error()};
    }
    ban.ip = std::move(*ip);
    ban.reason = std::move(*reason);
    ban.source = std::move(*source);
    ban.expiresEpochSeconds = *expires;
    return ban;
}

Parsed<ManagedOperator> parseOperator(const JsonValue& value) {
    const JsonObject* object = asObject(value);
    const JsonValue* player = object == nullptr ? nullptr : field(*object, "player");
    if (player == nullptr) {
        return std::unexpected{std::string{"No key player in MapLike[operator]"}};
    }
    auto parsed = parsePlayer(*player);
    if (!parsed.has_value()) {
        return std::unexpected{parsed.error()};
    }
    ManagedOperator entry{std::move(*parsed), std::nullopt, std::nullopt};
    if (const JsonValue* level = field(*object, "permissionLevel"); level != nullptr) {
        const auto* number = level->get<std::int64_t>();
        if (number == nullptr || *number < 0 || *number > 4) {
            return std::unexpected{std::string{"Value must be within range [0;4]"}};
        }
        entry.permissionLevel = static_cast<std::int32_t>(*number);
    }
    if (const JsonValue* bypass = field(*object, "bypassesPlayerLimit"); bypass != nullptr) {
        const auto* flag = bypass->get<bool>();
        if (flag == nullptr) {
            return std::unexpected{std::string{"Not a boolean: bypassesPlayerLimit"}};
        }
        entry.bypassesPlayerLimit = *flag;
    }
    return entry;
}

Parsed<ManagedGameRule> parseUntypedRule(const JsonValue& value) {
    const JsonObject* object = asObject(value);
    const JsonValue* key = object == nullptr ? nullptr : field(*object, "key");
    const auto* keyText = key == nullptr ? nullptr : key->get<std::string>();
    if (keyText == nullptr) {
        return std::unexpected{std::string{"No key key in MapLike[untyped_game_rule]"}};
    }
    const JsonValue* given = field(*object, "value");
    if (given == nullptr) {
        return std::unexpected{std::string{"No key value in MapLike[untyped_game_rule]"}};
    }
    ManagedGameRule rule;
    rule.key = *keyText;
    if (const auto* flag = given->get<bool>(); flag != nullptr) {
        rule.boolean = true;
        rule.flag = *flag;
    } else if (const auto* number = given->get<std::int64_t>(); number != nullptr) {
        rule.number = *number;
    } else {
        return std::unexpected{std::string{"Not a boolean or an integer: value"}};
    }
    return rule;
}

} // namespace basalt::server::remote::managementDto
