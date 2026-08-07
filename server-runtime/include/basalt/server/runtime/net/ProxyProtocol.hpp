#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace basalt::server::runtime::net::proxyProtocol {

struct Origin {
    std::string address;
    std::uint16_t port = 0;
};

/// nullopt for a LOCAL or UNKNOWN header, which carries no client address.
using Parsed = std::optional<Origin>;

/// Reads one HAProxy header (v1 or v2) through `readSome`, which returns 0 on end of stream.
[[nodiscard]] std::expected<Parsed, std::string>
readHeader(const std::function<std::size_t(std::span<std::uint8_t>)>& readSome);

} // namespace basalt::server::runtime::net::proxyProtocol
