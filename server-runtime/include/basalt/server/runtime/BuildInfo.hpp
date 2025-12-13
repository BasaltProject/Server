#pragma once

#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::buildInfo {

/// What this build is, for the console banner, `/version` and the server list.
///
/// The version comes from the build (`BASALT_VERSION`, which also names the binary), so a release
/// and what it says about itself cannot drift apart. The other two are the target this source tree
/// implements, which is the same fact the protocol module already carries.

#ifndef BASALT_VERSION
#define BASALT_VERSION "0.0.0-dev"
#endif

inline constexpr std::string_view kVersion = BASALT_VERSION;

inline constexpr std::string_view kMinecraftVersion = "26.2";

inline constexpr std::int32_t kProtocolVersion = wire::multiversion::protocolVersions::kNative;

} // namespace basalt::server::runtime::buildInfo
