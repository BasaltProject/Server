#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::remote {

struct StoredToken {
    std::string token;
    bool created = false;
};

/// The token is state, not configuration: it is generated once and kept owner-only.
[[nodiscard]] std::expected<StoredToken, std::string> loadOrCreateToken(const std::string& path);

/// The management secret: 40 letters and digits, as the vanilla management server asks for.
[[nodiscard]] bool isValidManagementSecret(std::string_view secret);

[[nodiscard]] std::expected<StoredToken, std::string> loadOrCreateManagementSecret(const std::string& path);

} // namespace basalt::server::remote
