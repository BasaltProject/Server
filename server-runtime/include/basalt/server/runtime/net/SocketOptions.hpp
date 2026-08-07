#pragma once

#include <cstdint>

namespace basalt::server::runtime::net {

/// Sets TCP_NODELAY on an accepted socket, as netty does for vanilla; false when the system refused.
[[nodiscard]] bool setTcpNoDelay(std::int64_t handle, bool enabled) noexcept;

/// What the socket reports for TCP_NODELAY, false when it cannot be read.
[[nodiscard]] bool tcpNoDelay(std::int64_t handle) noexcept;

} // namespace basalt::server::runtime::net
