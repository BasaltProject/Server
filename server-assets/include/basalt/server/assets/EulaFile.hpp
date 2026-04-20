#pragma once

#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::assets {
// parity: `net.minecraft.server.Eula`.
namespace eulaFile {
inline constexpr std::string_view kPath = "eula.toml";

inline constexpr std::string_view kLegacyPath = "eula.txt";

inline constexpr std::int32_t kRejectedExitCode = 78;

enum class Outcome : std::uint8_t {
    Accepted,
    Rejected,
    JustCreated,
};

[[nodiscard]] Outcome check(bool acceptFlag, const wire::common::Logger& logger);

[[nodiscard]] bool isAccepted(std::string_view content);
} // namespace eulaFile
} // namespace basalt::server::assets
