#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

/// Appends the missing options of `defaultContents` as comments and returns the keys added.
[[nodiscard]] std::string appendMissingOptions(std::string_view path, std::string_view fileName,
                                               std::string_view defaultContents,
                                               const wire::common::toml::TomlTable& values,
                                               const wire::common::Logger& logger);

} // namespace basalt::server::runtime::config
