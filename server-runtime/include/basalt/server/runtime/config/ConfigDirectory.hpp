#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

class ConfigDirectory {
public:
    static constexpr std::string_view kDefaultRoot = "config";

    explicit ConfigDirectory(std::string root = std::string{kDefaultRoot},
                             wire::common::Logger logger = wire::common::Logger{"Config"})
        : root_(std::move(root)), logger_(std::move(logger)) {}

    [[nodiscard]] const std::string& root() const noexcept { return root_; }

    [[nodiscard]] std::string pathOf(std::string_view fileName) const;

    [[nodiscard]] std::expected<wire::common::toml::TomlTable, std::string>
    load(std::string_view fileName, std::string_view defaultContents) const;

private:
    std::string root_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::runtime::config
