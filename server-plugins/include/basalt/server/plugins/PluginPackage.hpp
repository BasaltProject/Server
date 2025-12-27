#pragma once

#include <basalt/server/plugins/PluginManifest.hpp>
#include <basalt/wire/io/ZipReader.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

/// Something the loader could not get past, said in one line that names the package.
struct PluginLoadError {
    std::string message;
};

/// Where the parts of a package live. One place, so nothing else has to spell these out.
namespace packageLayout {

inline constexpr std::string_view kManifest = PluginManifest::kFileName;
inline constexpr std::string_view kSource = "src/";
inline constexpr std::string_view kResources = "resources/";
inline constexpr std::string_view kConfigFile = "resources/config.toml";
inline constexpr std::string_view kConfigDirectory = "resources/config/";
inline constexpr std::string_view kLang = "resources/lang/";
inline constexpr std::string_view kAssets = "resources/assets/";
/// The extension of a built package.
inline constexpr std::string_view kExtension = ".bspkg";

/// The file a Lua module name points at, `src/` included: `util.money` is `src/util/money.lua`.
///
/// Dots and no extension, which is how Lua has always named a module. There is deliberately one
/// spelling and not several: `require` caches by the name it was handed, so two names for one file
/// would run it twice and hand each caller its own copy of what the module keeps.
[[nodiscard]] std::optional<std::string> moduleFile(std::string_view module);

} // namespace packageLayout

/// The files of one plugin, however they are stored.
///
/// A packed `.bspkg` and an unpacked folder are the same thing to everything above this: the folder
/// form exists so that iterating on a plugin is saving a file, not building a zip.
class PluginPackage {
public:
    PluginPackage(const PluginPackage&) = delete;
    PluginPackage& operator=(const PluginPackage&) = delete;
    virtual ~PluginPackage() = default;

    /// Where it came from, for log lines that have to be actionable.
    [[nodiscard]] const std::string& origin() const noexcept { return origin_; }

    /// Whether this is a folder being edited rather than a built package.
    [[nodiscard]] virtual bool editable() const noexcept = 0;

    /// Where this plugin's own files already are, for a folder being worked on.
    ///
    /// A folder package is its own working folder: its `resources/` is what a built package would
    /// have been copied out to, so it is used where it lies instead of being copied next to itself.
    /// Absent for a built package, which gets `plugins/<id>/`.
    [[nodiscard]] virtual std::optional<std::string> workingFolder() const { return std::nullopt; }

    /// The bytes at `path`, or nothing when there is nothing there. Paths are package-relative.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> read(std::string_view path) const = 0;

    /// Every path under `prefix`, package-relative, in a stable order.
    [[nodiscard]] virtual std::vector<std::string> list(std::string_view prefix) const = 0;

    [[nodiscard]] std::optional<std::string> readText(std::string_view path) const;

    [[nodiscard]] bool exists(std::string_view path) const { return read(path).has_value(); }

protected:
    explicit PluginPackage(std::string origin) : origin_(std::move(origin)) {}

private:
    std::string origin_;
};

/// A plugin being worked on: a folder under `plugins/`, read from disk every time.
class FolderPackage final : public PluginPackage {
public:
    explicit FolderPackage(std::string root);

    [[nodiscard]] bool editable() const noexcept override { return true; }

    [[nodiscard]] std::optional<std::string> workingFolder() const override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path) const override;

    [[nodiscard]] std::vector<std::string> list(std::string_view prefix) const override;

private:
    void walk(const std::string& relative, std::vector<std::string>& into) const;

    std::string root_;
};

/// A built package: one zip, read once, entries pulled out as they are asked for.
class ArchivePackage final : public PluginPackage {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<ArchivePackage>, PluginLoadError>
    open(std::string path, std::vector<std::uint8_t> bytes);

    [[nodiscard]] bool editable() const noexcept override { return false; }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path) const override;

    [[nodiscard]] std::vector<std::string> list(std::string_view prefix) const override;

private:
    ArchivePackage(std::string path, wire::io::ZipReader zip);

    wire::io::ZipReader zip_;
};

/// What is sitting in `plugins/`, folders and packages alike.
///
/// A folder and a package with the same id is not resolved by picking one: it is refused, because
/// guessing which one the operator meant is how a server ends up running last week's code.
[[nodiscard]] std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError>
discoverPackages(const std::string& directory);

} // namespace basalt::server::plugins
