#pragma once

#include <basalt/server/remote/ManagementBackend.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::remote::managementDto {

using wire::common::JsonObject;
using wire::common::JsonValue;

template <typename T>
using Parsed = std::expected<T, std::string>;

struct IncomingIpBan {
    std::optional<ManagedPlayer> player;
    std::optional<std::string> ip;
    std::optional<std::string> reason;
    std::optional<std::string> source;
    std::optional<std::int64_t> expiresEpochSeconds;
};

[[nodiscard]] std::string formatInstant(std::int64_t epochSeconds);
[[nodiscard]] std::optional<std::int64_t> parseInstant(std::string_view text);

[[nodiscard]] bool looksLikeAddress(std::string_view text);

[[nodiscard]] JsonValue encodePlayer(const ManagedPlayer& player);
[[nodiscard]] JsonValue encodeUserBan(const ManagedUserBan& ban);
[[nodiscard]] JsonValue encodeIpBan(const ManagedIpBan& ban);
[[nodiscard]] JsonValue encodeOperator(const ManagedOperator& entry);
[[nodiscard]] JsonValue encodeTypedRule(const ManagedGameRule& rule);
[[nodiscard]] JsonValue encodeServerState(bool started, const std::vector<ManagedPlayer>& players,
                                          std::string_view versionName, std::int32_t protocol);

[[nodiscard]] Parsed<ManagedPlayer> parsePlayer(const JsonValue& value);
[[nodiscard]] Parsed<ManagedMessage> parseMessage(const JsonValue& value);
[[nodiscard]] Parsed<ManagedUserBan> parseUserBan(const JsonValue& value);
[[nodiscard]] Parsed<ManagedIpBan> parseIpBan(const JsonValue& value);
[[nodiscard]] Parsed<IncomingIpBan> parseIncomingIpBan(const JsonValue& value);
[[nodiscard]] Parsed<ManagedOperator> parseOperator(const JsonValue& value);
[[nodiscard]] Parsed<ManagedGameRule> parseUntypedRule(const JsonValue& value);
[[nodiscard]] Parsed<std::string> parseString(const JsonValue& value);

template <typename T>
[[nodiscard]] Parsed<std::vector<T>> parseList(const JsonValue& value,
                                               const std::function<Parsed<T>(const JsonValue&)>& each) {
    const auto* list = value.get<std::vector<JsonValue>>();
    if (list == nullptr) {
        return std::unexpected{std::string{"Not a list"}};
    }
    std::vector<T> out;
    for (const JsonValue& element : *list) {
        auto parsed = each(element);
        if (!parsed.has_value()) {
            return std::unexpected{std::move(parsed.error())};
        }
        out.push_back(std::move(*parsed));
    }
    return out;
}

template <typename T>
[[nodiscard]] JsonValue encodeList(const std::vector<T>& items, JsonValue (*encode)(const T&)) {
    std::vector<JsonValue> out;
    out.reserve(items.size());
    for (const T& item : items) {
        out.push_back(encode(item));
    }
    return JsonValue::ofArray(std::move(out));
}

} // namespace basalt::server::remote::managementDto
