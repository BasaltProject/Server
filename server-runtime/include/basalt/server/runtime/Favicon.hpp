#pragma once

#include <cstdint>
#include <span>

namespace basalt::server::runtime::favicon {

/// The default server list favicon: the Basalt block icon, 64x64 PNG.
///
/// Embedded base64 rather than a file: the status response needs it before anything has read a
/// directory, and a server list icon that depends on where the binary was started from is one more
/// thing to get wrong.
[[nodiscard]] std::span<const std::uint8_t> bytes();

} // namespace basalt::server::runtime::favicon
