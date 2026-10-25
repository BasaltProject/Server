#include <basalt/server/remote/ApiToken.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <mbedtls/entropy.h>
#include <sstream>

namespace basalt::server::remote {
namespace {

constexpr std::size_t kTokenBytes = 32;

[[nodiscard]] std::string trimmed(std::string text) {
    while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ') {
        text.pop_back();
    }
    return text;
}

} // namespace

std::expected<StoredToken, std::string> loadOrCreateToken(const std::string& path) {
    namespace fs = std::filesystem;
    if (std::ifstream existing{path}; existing) {
        std::ostringstream content;
        content << existing.rdbuf();
        std::string token = trimmed(content.str());
        if (token.size() >= 16) {
            return StoredToken{std::move(token), false};
        }
    }
    std::array<unsigned char, kTokenBytes> bytes{};
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    const int code = mbedtls_entropy_func(&entropy, bytes.data(), bytes.size());
    mbedtls_entropy_free(&entropy);
    if (code != 0) {
        return std::unexpected("could not gather randomness for the API token");
    }
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string token;
    for (const unsigned char byte : bytes) {
        token.push_back(kDigits[byte >> 4]);
        token.push_back(kDigits[byte & 0x0FU]);
    }
    std::error_code error;
    fs::create_directories(fs::path{path}.parent_path(), error);
    {
        std::ofstream out{path, std::ios::trunc};
        if (!out) {
            return std::unexpected("could not write " + path);
        }
        fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, error);
        out << token << '\n';
    }
    return StoredToken{std::move(token), true};
}

} // namespace basalt::server::remote
