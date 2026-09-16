#pragma once

#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>

namespace basalt::server::runtime::entity {
class ServerEntity;
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime::bedrock {

/// What one entity looks like to a client of the other edition: the name it draws it under, the
/// flags it draws it by, the line over its head and which of its kind it is.
///
/// Its own file rather than a lambda where the world runs: every mob that is drawn differently on
/// the two editions adds a line here, and the list only grows.
[[nodiscard]] Seen seenOf(const entity::ServerEntity& entity);

} // namespace basalt::server::runtime::bedrock
