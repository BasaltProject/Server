#pragma once

#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::assets {

/// parity: `net.minecraft.server.Eula`, minus the prompt: no TTY detection, identical interactive,
/// headless or in a container. `--accept-eula` is a deliberate operator act and counts as acceptance.
///
/// TOML because the EULA asks for agreement, not for a file name, and vanilla's own body already
/// parses as TOML. `eula.txt` is still honoured and migrated once, since hosting panels write it.
namespace eulaFile {

inline constexpr std::string_view kPath = "eula.toml";

/// Vanilla's own name, still read so panels and migrated servers keep working.
inline constexpr std::string_view kLegacyPath = "eula.txt";

/// A stable, documented exit code for a rejected or missing EULA, so orchestrators and scripts can
/// tell it from a crash.
inline constexpr std::int32_t kRejectedExitCode = 78;

enum class Outcome : std::uint8_t {
    Accepted,
    /// The file was there and said no.
    Rejected,
    /// The file was not there at all, and has just been written saying no.
    JustCreated,
};

[[nodiscard]] Outcome check(bool acceptFlag, const wire::common::Logger& logger);

/// Accepts vanilla's spacing-free `eula=true` as readily as TOML's `eula = true`.
[[nodiscard]] bool isAccepted(std::string_view content);

} // namespace eulaFile

} // namespace basalt::server::assets
