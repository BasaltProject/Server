#pragma once

#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::datapack {

struct PackFiles {
    std::function<std::vector<std::string>()> list;
    std::function<std::optional<std::vector<std::uint8_t>>(std::string_view path)> read;
};

/// parity: `PathPackResources`.
[[nodiscard]] PackFiles directoryFiles(std::string root);

/// parity: `FilePackResources`.
[[nodiscard]] std::optional<PackFiles> zipFiles(const std::string& path);

[[nodiscard]] PackFiles subtreeFiles(const PackFiles& whole, std::string prefix,
                                     std::vector<std::string> excluded = {});

[[nodiscard]] bool validNamespace(std::string_view name);

[[nodiscard]] bool validPath(std::string_view path);

/// parity: `OverlayedPackResources`.
class PackContents {
public:
    PackContents(std::string packId, PackFiles files, const std::vector<std::string>& overlays,
                 const wire::common::Logger& logger);

    [[nodiscard]] const std::string& packId() const noexcept { return packId_; }

    [[nodiscard]] bool contains(const std::string& name) const { return index_.contains(name); }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const;

    [[nodiscard]] const std::map<std::string, std::string>& index() const noexcept { return index_; }

    /// parity: `getNamespaces`.
    [[nodiscard]] const std::set<std::string>& namespaces() const noexcept { return namespaces_; }

private:
    std::string packId_;
    PackFiles files_;
    std::map<std::string, std::string> index_;
    std::set<std::string> namespaces_;
};

} // namespace basalt::server::runtime::datapack
