#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::placeholder {

/// What the server can be asked about itself, read live rather than captured once.
struct ServerFacts {
    std::string versionName;
    std::int32_t protocolVersion = 0;
    /// Milliseconds since the epoch, which is what the uptime is measured from.
    std::int64_t startedAtMillis = 0;
    std::function<std::int32_t()> maxPlayers;
    /// Names of the players connected, by world, which answers both the count and the per-world one.
    std::function<std::map<std::string, std::int32_t>()> onlineByWorld;
    std::function<std::int64_t()> uniqueJoins;
    std::function<std::int32_t()> loadedChunks;
    std::function<std::optional<std::int64_t>()> liveHeapBytes;
    /// Ticks per second over one, five and fifteen minutes, or nothing before anything measured it.
    std::function<std::optional<std::array<double, 3>>()> tickRates;
    /// Share of one core this process is using, as a percentage, or nothing where the platform
    /// cannot say.
    std::function<std::optional<double>()> cpuPercent;
    /// The wall clock, which the uptime and `time_<format>` read. Swappable so a test has one.
    std::function<std::int64_t()> nowMillis;
};

/// `%basalt_server_*%`.
///
/// The ones this build cannot answer are registered all the same, with the reason: a server list
/// that asks for the TPS should be told that nothing measures it yet, not that it invented a
/// placeholder.
[[nodiscard]] wire::common::placeholder::PlaceholderProvider serverPlaceholders(ServerFacts facts);

[[nodiscard]] wire::common::placeholder::PlaceholderCatalog serverPlaceholderNames();

/// parity-free helpers the provider is written from, exposed because the tab list formats the same
/// way and a test can reach them without a whole server.
namespace serverPlaceholderFormat {

/// Two decimals, without pulling in a formatting library for one number.
[[nodiscard]] std::string twoDecimals(double value);

[[nodiscard]] std::string uptime(std::int64_t elapsedMillis);

/// The subset of `SimpleDateFormat` worth supporting: `yyyy MM dd HH mm ss`, anything else literal.
///
/// Not the whole thing, and deliberately: the rest of that format language is eras, week numbers,
/// time zones and locale-dependent month names, none of which a server list line needs, and half of
/// which would be wrong without a locale to render them in.
[[nodiscard]] std::string localTime(std::string_view pattern, std::int64_t epochMillis);

} // namespace serverPlaceholderFormat

} // namespace basalt::server::runtime::placeholder
