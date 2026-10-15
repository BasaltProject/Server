#pragma once

#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::assets {
namespace langAssets {
[[nodiscard]] std::expected<wire::text::LangTable, AssetError> load(const std::string& jarPath,
                                                                    std::string_view locale = "en_us");

[[nodiscard]] wire::text::LangTable loadCachedOrEmpty(const std::string& versionDir, std::string_view locale = "en_us");
} // namespace langAssets
} // namespace basalt::server::assets
