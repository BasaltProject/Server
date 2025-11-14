#pragma once

#include <basalt/server/plugins/PluginPackage.hpp>

#include <expected>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::plugins {

/// The folder one plugin owns, `plugins/<id>/`, and what the package puts in it.
///
/// No shape is imposed: whatever the package has under `resources/`, at whatever depth, appears here
/// with the same shape. A layout forced on everyone to suit a few is how a folder ends up with three
/// empty directories in it.
///
/// A file already there is left alone, so an update brings new files and never touches what the
/// operator edited.
class PluginFolder {
public:
    /// What was written, one line each, for the loader to log.
    using Report = std::function<void(std::string_view)>;

    PluginFolder(const PluginPackage& package, const PluginManifest& manifest, std::string root)
        : package_(&package), manifest_(&manifest), root_(std::move(root)) {}

    /// Puts the package's `resources/` into the folder, keeping its shape.
    void materialize(const Report& report) const;

    /// Every file in the folder, by path.
    ///
    /// The addressable ones are checked for a repeated name here rather than where they are used:
    /// `config.chat` means `chat.toml` wherever it is, so two of them is a question with no answer,
    /// and finding that out at load with both paths named beats finding out from the wrong value.
    [[nodiscard]] std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> index() const;

private:
    [[nodiscard]] bool isPrivate(std::string_view relative) const;

    const PluginPackage* package_;
    const PluginManifest* manifest_;
    std::string root_;
};

} // namespace basalt::server::plugins
