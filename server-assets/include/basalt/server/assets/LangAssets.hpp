#pragma once

#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::assets {

/// The official language table, which is what lets the server console print the same messages it
/// sends to clients as sentences rather than as translation keys.
///
/// A client resolves `commands.gamemode.success.self` against its own language; nothing does that
/// for a log file, so the server has to. The file comes out of the server jar, under
/// `assets/minecraft/lang/`: it ships there as well as in `client.jar`, so no extra download is
/// needed. Only the outer bundler archive lacks it, which is what made an earlier version of this
/// reach for `client.jar` instead.
namespace langAssets {

/// Extracts a locale's lang JSON straight from a jar, for a jar that has not been unpacked.
[[nodiscard]] std::expected<wire::text::LangTable, AssetError> load(const std::string& jarPath,
                                                                    std::string_view locale = "en_us");

/// The locale table already unpacked into `versionDir`, or an empty one if it is not there.
///
/// Empty is a cosmetic loss, the console printing keys, never a failure: a cache written before
/// language files were unpacked degrades instead of refusing to start.
[[nodiscard]] wire::text::LangTable loadCachedOrEmpty(const std::string& versionDir, std::string_view locale = "en_us");

} // namespace langAssets

} // namespace basalt::server::assets
