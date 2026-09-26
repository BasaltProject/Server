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

/// Every file under one pack root, named by its path from that root with `/` separators.
struct PackFiles {
    std::function<std::vector<std::string>()> list;
    std::function<std::optional<std::vector<std::uint8_t>>(std::string_view path)> read;
};

/// A pack that is a folder on disk. parity: `PathPackResources`.
[[nodiscard]] PackFiles directoryFiles(std::string root);

/// A pack that is a `.zip`, read into memory once. Nothing when the archive does not open.
/// parity: `FilePackResources`.
[[nodiscard]] std::optional<PackFiles> zipFiles(const std::string& path);

/// The part of `whole` under `prefix`, renamed relative to it, minus whatever lies under `excluded`.
/// How the jar's own `data/` becomes the vanilla pack and its `datapacks/<id>/` the feature packs.
[[nodiscard]] PackFiles subtreeFiles(const PackFiles& whole, std::string prefix,
                                     std::vector<std::string> excluded = {});

/// Whether a namespace is one vanilla accepts, `[a-z0-9_.-]`.
[[nodiscard]] bool validNamespace(std::string_view name);

/// Whether a resource path is one vanilla accepts, `[a-z0-9_.-/]`.
[[nodiscard]] bool validPath(std::string_view path);

/// One opened pack: its `data/` entries with the applicable overlays laid over them.
///
/// parity: `OverlayedPackResources`: the last overlay listed wins, then the ones before it, then the
/// pack itself. Names are `data/<namespace>/<path>`, whichever layer they come from.
class PackContents {
public:
    PackContents(std::string packId, PackFiles files, const std::vector<std::string>& overlays,
                 const wire::common::Logger& logger);

    [[nodiscard]] const std::string& packId() const noexcept { return packId_; }

    [[nodiscard]] bool contains(const std::string& name) const { return index_.contains(name); }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const;

    /// Every name, sorted.
    [[nodiscard]] const std::map<std::string, std::string>& index() const noexcept { return index_; }

    /// The namespaces the pack's `data/` holds. parity: `getNamespaces`.
    [[nodiscard]] const std::set<std::string>& namespaces() const noexcept { return namespaces_; }

private:
    std::string packId_;
    PackFiles files_;
    /// Name to the path it is read from, which differs when an overlay supplies it.
    std::map<std::string, std::string> index_;
    std::set<std::string> namespaces_;
};

} // namespace basalt::server::runtime::datapack
