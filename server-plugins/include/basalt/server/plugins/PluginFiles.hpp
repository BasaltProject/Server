#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins::fileService {

enum class Kind : std::int32_t { File = 0, Directory = 1, Other = 2 };

struct Stat {
    Kind kind{Kind::File};
    std::int64_t size{};
    std::int64_t modifiedMillis{};
};

struct Capabilities {
    bool atomicMove{};
    bool hardLinks{};
};

[[nodiscard]] std::optional<Stat> stat(std::string_view path);

/// Entries under a directory, each as `f:`, `d:` or `o:` plus the path relative to it, parents before children.
[[nodiscard]] std::vector<std::string> walk(std::string_view directory);

[[nodiscard]] std::optional<std::vector<std::string>> list(std::string_view directory);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path);
/// Writes through a sibling temporary file and a rename, so a reader never sees a partial file.
[[nodiscard]] bool write(std::string_view path, const std::vector<std::uint8_t>& bytes);
[[nodiscard]] bool makeDirectories(std::string_view path);
[[nodiscard]] bool copyFile(std::string_view from, std::string_view to);
[[nodiscard]] bool hardLink(std::string_view from, std::string_view to);
[[nodiscard]] bool move(std::string_view from, std::string_view to, bool atomic);
[[nodiscard]] bool remove(std::string_view path);
[[nodiscard]] bool removeTree(std::string_view path);
[[nodiscard]] Capabilities capabilities(std::string_view directory);

} // namespace basalt::server::plugins::fileService
