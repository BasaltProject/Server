#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <optional>
#include <string>

namespace basalt::server::runtime::config {

using wire::common::toml::TomlError;
using wire::common::toml::TomlTable;

std::string ConfigDirectory::pathOf(std::string_view fileName) const { return root_ + "/" + std::string{fileName}; }

std::expected<TomlTable, std::string> ConfigDirectory::load(std::string_view fileName,
                                                            std::string_view defaultContents) const {
    const std::string path = pathOf(fileName);
    if (!wire::io::fileIO::exists(path)) {
        wire::io::fileIO::createDirectories(root_);
        wire::io::fileIO::writeText(path, defaultContents);
        logger_.info([&path] { return path + " not found, generating one with default values."; });
    }
    const std::optional<std::string> text = wire::io::fileIO::readText(path);
    // Only a real read failure gets here, which is why no test stages it: the file was either
    // already there or has just been written. Without the check the read below is undefined
    // behaviour rather than a message, which is the whole reason it stays.
    if (!text.has_value()) {
        return std::unexpected(std::string{"Failed to read "} + path);
    }
    const std::expected<TomlTable, TomlError> parsed = wire::common::toml::parse(*text);
    if (!parsed.has_value()) {
        // Named rather than swallowed: with several files, "the configuration is broken" is not a
        // useful thing to tell someone.
        return std::unexpected(
            path + " is not valid TOML: " + std::string{wire::common::toml::tomlErrorMessage(parsed.error())});
    }
    return *parsed;
}

} // namespace basalt::server::runtime::config
