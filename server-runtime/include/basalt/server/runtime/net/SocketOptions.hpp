#pragma once

#include <cstdint>

namespace basalt::server::runtime::net {

/// Sets TCP_NODELAY on an accepted socket, as netty does for vanilla; false when the system refused.
[[nodiscard]] bool setTcpNoDelay(std::int64_t handle, bool enabled) noexcept;

/// What the socket reports for TCP_NODELAY, false when it cannot be read.
[[nodiscard]] bool tcpNoDelay(std::int64_t handle) noexcept;

/// Makes a blocking read give up after this long; zero removes the limit.
[[nodiscard]] bool setReceiveTimeout(std::int64_t handle, std::int32_t millis) noexcept;

/// Makes a blocking read give up after this long; zero removes the limit.
bool setReceiveTimeout(std::int64_t handle, std::int32_t millis) noexcept;

} // namespace basalt::server::runtime::net
