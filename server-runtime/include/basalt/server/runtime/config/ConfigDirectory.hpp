#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

/// The `config/` directory: one file per domain, all TOML, all in one place. What makes Paper's
/// configuration hard is not the number of files but that they are split without a rule, in two
/// formats, with the same option in two places. A new file has to earn being its own domain rather
/// than one more table in an existing one.
///
/// A missing file is generated from its commented default. A file that does not parse is an error
/// the caller stops the server over: falling back to defaults would mean a typo in `groups.toml`
/// quietly granting nobody any permissions.
class ConfigDirectory {
public:
    static constexpr std::string_view kDefaultRoot = "config";

    explicit ConfigDirectory(std::string root = std::string{kDefaultRoot},
                             wire::common::Logger logger = wire::common::Logger{"Config"})
        : root_(std::move(root)), logger_(std::move(logger)) {}

    [[nodiscard]] const std::string& root() const noexcept { return root_; }

    [[nodiscard]] std::string pathOf(std::string_view fileName) const;

    /// Reads one configuration file, writing `defaultContents` first if it is not there yet.
    ///
    /// `defaultContents` is expected to be the commented default: the file an operator opens on
    /// first run should explain itself, not be an empty skeleton.
    ///
    /// The failure carries the path, because with several files "the configuration is broken" is
    /// not a useful thing to tell someone.
    /// Reads have a 256 KiB ceiling; an oversized file fails instead of falling back to defaults.
    [[nodiscard]] std::expected<wire::common::toml::TomlTable, std::string>
    load(std::string_view fileName, std::string_view defaultContents) const;

private:
    std::string root_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::runtime::config
