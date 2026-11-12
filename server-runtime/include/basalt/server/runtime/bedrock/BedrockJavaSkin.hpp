#pragma once

#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>

#include <optional>
#include <span>

namespace basalt::server::runtime::bedrock {

/// The skin a Java account carries in its profile, fetched and decoded; nullopt when it has none or cannot be read.
[[nodiscard]] std::optional<wire::bedrock::SkinInfo>
javaSkinOf(std::span<const wire::protocol::login::GameProfileProperty> properties);

} // namespace basalt::server::runtime::bedrock
