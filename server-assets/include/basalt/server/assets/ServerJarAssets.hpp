#pragma once

#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::assets {
class ServerJarAssets {
public:
    using Source = std::function<std::expected<std::string, AssetError>()>;

    [[nodiscard]] static std::expected<ServerJarAssets, AssetError>
    open(std::string_view versionId, std::string dataRoot, const Source& source,
         wire::common::Logger logger = wire::common::Logger{"Assets"});

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> readEntry(std::string_view name) const;

    [[nodiscard]] std::expected<std::string, AssetError> readText(std::string_view name) const;

    [[nodiscard]] std::vector<std::string> entriesUnder(std::string_view prefix) const;

    [[nodiscard]] std::vector<wire::common::Identifier> dataIds(std::string_view directory,
                                                                std::string_view extension) const;

private:
    ServerJarAssets(std::string dataRoot, std::vector<std::string> entryNames, wire::common::Logger logger)
        : dataRoot_(std::move(dataRoot)), entryNames_(std::move(entryNames)), logger_(std::move(logger)) {}

    [[nodiscard]] std::string pathFor(std::string_view name) const;

    std::string dataRoot_;
    std::vector<std::string> entryNames_;
    wire::common::Logger logger_;
};
} // namespace basalt::server::assets
