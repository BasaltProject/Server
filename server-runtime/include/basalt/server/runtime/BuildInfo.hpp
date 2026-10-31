#pragma once

#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::buildInfo {

extern const std::string_view kVersion;

inline constexpr std::string_view kMinecraftVersion = "26.3";

inline constexpr std::int32_t kProtocolVersion = wire::multiversion::protocolVersions::kNative;

} // namespace basalt::server::runtime::buildInfo
