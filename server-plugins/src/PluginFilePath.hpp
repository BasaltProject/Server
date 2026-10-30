#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::plugins::pluginApi {
// Return the resolved local target, not a lexical path that can follow an escaping symlink.
inline std::optional<std::string> pluginFilePath(std::string_view folder, std::string_view relative) {
    if (relative.find('\0') != std::string_view::npos || relative.find('\\') != std::string_view::npos ||
        relative.find("..") != std::string_view::npos || relative.find(':') != std::string_view::npos)
        return std::nullopt;
    const std::filesystem::path suffix{relative};
    if (suffix.has_root_path())
        return std::nullopt;
    std::error_code error;
    const auto root =
        std::filesystem::weakly_canonical(std::filesystem::absolute(std::filesystem::path{folder}), error);
    if (error)
        return std::nullopt;
    const auto target = std::filesystem::weakly_canonical(root / suffix, error);
    if (error)
        return std::nullopt;
    auto from = root.begin(), at = target.begin();
    for (; from != root.end(); ++from, ++at)
        if (at == target.end() || *at != *from)
            return std::nullopt;
    return target.string();
}
} // namespace basalt::server::plugins::pluginApi
