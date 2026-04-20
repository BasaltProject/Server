#pragma once

#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::buildInfo {

#ifndef BASALT_VERSION
#define BASALT_VERSION "0.0.0-dev"
#endif

inline constexpr std::string_view kVersion = BASALT_VERSION;

inline constexpr std::string_view kMinecraftVersion = "26.3";

inline constexpr std::int32_t kProtocolVersion = wire::multiversion::protocolVersions::kNative;

} // namespace basalt::server::runtime::buildInfo
