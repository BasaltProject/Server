#pragma once

#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>

namespace basalt::server::runtime::entity {
class ServerEntity;
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime::bedrock {

[[nodiscard]] Seen seenOf(const entity::ServerEntity& entity);

} // namespace basalt::server::runtime::bedrock
