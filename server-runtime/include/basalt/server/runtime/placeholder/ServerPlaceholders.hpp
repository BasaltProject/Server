#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::placeholder {

struct ServerFacts {
    std::string versionName;
    std::int32_t protocolVersion = 0;
    std::int64_t startedAtMillis = 0;
    std::function<std::int32_t()> maxPlayers;
    std::function<std::map<std::string, std::int32_t>()> onlineByWorld;
    std::function<std::int64_t()> uniqueJoins;
    std::function<std::int32_t()> loadedChunks;
    std::function<std::optional<std::int64_t>()> liveHeapBytes;
    std::function<std::optional<std::array<double, 3>>()> tickRates;
    std::function<std::optional<double>()> cpuPercent;
    std::function<std::int64_t()> nowMillis;
};

[[nodiscard]] wire::common::placeholder::PlaceholderProvider serverPlaceholders(ServerFacts facts);

[[nodiscard]] wire::common::placeholder::PlaceholderCatalog serverPlaceholderNames();

namespace serverPlaceholderFormat {

[[nodiscard]] std::string twoDecimals(double value);

[[nodiscard]] std::string uptime(std::int64_t elapsedMillis);

[[nodiscard]] std::string localTime(std::string_view pattern, std::int64_t epochMillis);

} // namespace serverPlaceholderFormat

} // namespace basalt::server::runtime::placeholder
