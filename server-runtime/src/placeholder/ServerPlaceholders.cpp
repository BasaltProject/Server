#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/placeholder/ServerPlaceholders.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <format>
#include <numeric>
#include <utility>

namespace basalt::server::runtime::placeholder {
namespace {

using wire::common::placeholder::PlaceholderValue;

constexpr std::int64_t kMillisPerSecond = 1000;
constexpr std::int64_t kBytesPerMegabyte = 1048576;

/// The placeholders that are real, spelled right, and cannot be answered by this build yet.
const std::map<std::string, std::string, std::less<>>& notImplementedReasons() {
    static const std::map<std::string, std::string, std::less<>> kReasons{
        {"ram_free", "the native runtime has no fixed heap: only what is live after a collection is known"},
        {"ram_total", "the native runtime has no fixed heap: only what is live after a collection is known"},
        {"ram_max", "the native runtime has no fixed heap: only what is live after a collection is known"},
        {"name", "there is no server name setting yet"},
        {"countdown", "counting down to a date is not implemented yet"},
        {"has_whitelist", "there is no whitelist yet"},
        {"total_entities", "there is no entity system yet"},
        {"total_living_entities", "there is no entity system yet"}};
    return kReasons;
}

const std::vector<std::string>& answeredNames() {
    static const std::vector<std::string> kNames{
        "cpu",           "max_players",  "online", "protocol",       "ram_used",      "total_chunks",
        "tps",           "tps_1",        "tps_15", "tps_15_colored", "tps_1_colored", "tps_5",
        "tps_5_colored", "unique_joins", "uptime", "uptime_seconds", "version"};
    return kNames;
}

std::int32_t onlineTotal(const ServerFacts& facts) {
    const std::map<std::string, std::int32_t> byWorld = facts.onlineByWorld();
    return std::accumulate(byWorld.begin(), byWorld.end(), 0,
                           [](std::int32_t total, const auto& entry) { return total + entry.second; });
}

PlaceholderValue tps(const ServerFacts& facts, std::size_t window) {
    const std::optional<std::array<double, 3>> rates = facts.tickRates();
    if (!rates.has_value()) {
        return PlaceholderValue::unavailable("nothing has measured the tick rate yet");
    }
    return PlaceholderValue::of(serverPlaceholderFormat::twoDecimals((*rates)[std::min(window, std::size_t{2})]));
}

/// The same number wrapped in a colour, as MiniMessage.
///
/// It is markup rather than a component because placeholders are substituted *before* the text is
/// parsed, which is what lets a colour from here blend into whatever the operator wrote around it.
PlaceholderValue coloured(PlaceholderValue value) {
    const std::string* text = value.textOrNull();
    if (text == nullptr) {
        return value;
    }
    double number = 0.0;
    try {
        std::size_t consumed = 0;
        number = std::stod(*text, &consumed);
        if (consumed != text->size()) {
            return value;
        }
    } catch (...) {
        return value;
    }
    const std::string_view colour = number >= 18.0 ? "green" : (number >= 15.0 ? "yellow" : "red");
    return PlaceholderValue::of(std::format("<{0}>{1}</{0}>", colour, *text));
}

} // namespace

namespace serverPlaceholderFormat {

std::string twoDecimals(double value) {
    const auto rounded = static_cast<std::int64_t>(std::round(value * 100.0));
    return std::format("{}.{:02d}", rounded / 100, std::abs(rounded % 100));
}

std::string uptime(std::int64_t elapsedMillis) {
    const std::int64_t seconds = elapsedMillis / kMillisPerSecond;
    const std::int64_t days = seconds / 86400;
    const std::int64_t hours = (seconds / 3600) % 24;
    const std::int64_t minutes = (seconds / 60) % 60;
    std::string result;
    if (days > 0) {
        result += std::format("{}d ", days);
    }
    if (days > 0 || hours > 0) {
        result += std::format("{}h ", hours);
    }
    if (days > 0 || hours > 0 || minutes > 0) {
        result += std::format("{}m ", minutes);
    }
    return result + std::format("{}s", seconds % 60);
}

std::string localTime(std::string_view pattern, std::int64_t epochMillis) {
    // Through `localtime_r` rather than `std::chrono::zoned_time`, which this platform's standard
    // library does not carry.
    const auto now = static_cast<std::time_t>(epochMillis / kMillisPerSecond);
    std::tm local{};
    localtime_r(&now, &local);
    std::string result{pattern};
    const auto replaceAll = [&result](std::string_view token, const std::string& value) {
        for (std::size_t at = result.find(token); at != std::string::npos; at = result.find(token, at + value.size())) {
            result.replace(at, token.size(), value);
        }
    };
    replaceAll("yyyy", std::format("{:04d}", local.tm_year + 1900));
    replaceAll("MM", std::format("{:02d}", local.tm_mon + 1));
    replaceAll("dd", std::format("{:02d}", local.tm_mday));
    replaceAll("HH", std::format("{:02d}", local.tm_hour));
    replaceAll("mm", std::format("{:02d}", local.tm_min));
    replaceAll("ss", std::format("{:02d}", local.tm_sec));
    return result;
}

} // namespace serverPlaceholderFormat

wire::common::placeholder::PlaceholderProvider serverPlaceholders(ServerFacts facts) {
    return [facts = std::move(facts)](std::string_view params,
                                      const wire::common::placeholder::PlaceholderContext&) -> PlaceholderValue {
        const auto reason = notImplementedReasons().find(params);
        if (reason != notImplementedReasons().end()) {
            return notImplemented(reason->second);
        }
        if (params.starts_with("online_")) {
            const std::map<std::string, std::int32_t> byWorld = facts.onlineByWorld();
            const auto world = byWorld.find(std::string{params.substr(7)});
            return PlaceholderValue::of(static_cast<std::int64_t>(world == byWorld.end() ? 0 : world->second));
        }
        if (params.starts_with("countdown_")) {
            return notImplemented("counting down to a date is not implemented yet");
        }
        if (params.starts_with("time_")) {
            return PlaceholderValue::of(serverPlaceholderFormat::localTime(params.substr(5), facts.nowMillis()));
        }
        if (params == "version") {
            return PlaceholderValue::of(facts.versionName);
        }
        if (params == "protocol") {
            return PlaceholderValue::of(static_cast<std::int64_t>(facts.protocolVersion));
        }
        if (params == "online") {
            return PlaceholderValue::of(static_cast<std::int64_t>(onlineTotal(facts)));
        }
        if (params == "max_players") {
            return PlaceholderValue::of(static_cast<std::int64_t>(facts.maxPlayers()));
        }
        if (params == "unique_joins") {
            return PlaceholderValue::of(facts.uniqueJoins());
        }
        if (params == "total_chunks") {
            return PlaceholderValue::of(static_cast<std::int64_t>(facts.loadedChunks()));
        }
        if (params == "uptime") {
            return PlaceholderValue::of(serverPlaceholderFormat::uptime(facts.nowMillis() - facts.startedAtMillis));
        }
        if (params == "uptime_seconds") {
            return PlaceholderValue::of((facts.nowMillis() - facts.startedAtMillis) / kMillisPerSecond);
        }
        if (params == "ram_used") {
            // The one memory figure a native runtime with no fixed heap can honestly report.
            const std::optional<std::int64_t> live = facts.liveHeapBytes();
            return live.has_value() ? PlaceholderValue::of(*live / kBytesPerMegabyte) : PlaceholderValue::unavailable();
        }
        if (params == "cpu") {
            const std::optional<double> percent = facts.cpuPercent();
            return percent.has_value() ? PlaceholderValue::of(serverPlaceholderFormat::twoDecimals(*percent))
                                       : PlaceholderValue::unavailable();
        }
        if (params == "tps" || params == "tps_1") {
            return tps(facts, 0);
        }
        if (params == "tps_5") {
            return tps(facts, 1);
        }
        if (params == "tps_15") {
            return tps(facts, 2);
        }
        if (params == "tps_1_colored") {
            return coloured(tps(facts, 0));
        }
        if (params == "tps_5_colored") {
            return coloured(tps(facts, 1));
        }
        if (params == "tps_15_colored") {
            return coloured(tps(facts, 2));
        }
        return PlaceholderValue::unknown();
    };
}

wire::common::placeholder::PlaceholderCatalog serverPlaceholderNames() {
    return [] {
        std::vector<std::string> names = answeredNames();
        for (const auto& [name, reason] : notImplementedReasons()) {
            names.push_back(name);
        }
        names.emplace_back("online_<world>");
        names.emplace_back("time_<format>");
        std::ranges::sort(names);
        return names;
    };
}

} // namespace basalt::server::runtime::placeholder
