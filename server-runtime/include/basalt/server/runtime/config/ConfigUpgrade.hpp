#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

/// Appends, as comments, the options of `defaultContents` that `values` lacks in the sections this file keeps fixed.
/// Returns the keys added; the file is only ever appended to.
[[nodiscard]] std::string appendMissingOptions(std::string_view path, std::string_view fileName,
                                               std::string_view defaultContents,
                                               const wire::common::toml::TomlTable& values,
                                               const wire::common::Logger& logger);

} // namespace basalt::server::runtime::config
