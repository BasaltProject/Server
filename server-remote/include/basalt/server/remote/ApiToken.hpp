#pragma once

#include <expected>
#include <string>

namespace basalt::server::remote {

struct StoredToken {
    std::string token;
    bool created = false;
};

/// The token is state, not configuration: it is generated once and kept owner-only.
[[nodiscard]] std::expected<StoredToken, std::string> loadOrCreateToken(const std::string& path);

} // namespace basalt::server::remote
