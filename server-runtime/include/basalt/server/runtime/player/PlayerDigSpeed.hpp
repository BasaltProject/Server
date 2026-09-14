#pragma once

#include <basalt/server/runtime/world/RealBlockTags.hpp>

namespace basalt::wire::common {
class Identifier;
}

namespace basalt::server::registry {
struct BlockHardness;
}

namespace basalt::server::runtime::player {

class PlayerSession;

/// How much of `block` this player takes off in one tick; one or more breaks it at once. Eyes under
/// water come from the caller, which may be on a thread that cannot read the world.
///
/// parity: `BlockState.getDestroyProgress` over `Player.getDestroySpeed`.
[[nodiscard]] float destroyProgressPerTickFor(const PlayerSession& session, const wire::common::Identifier& block,
                                              const registry::BlockHardness* hardness,
                                              const world::realBlockTags::TagMembers* blockTags, bool underwater);

} // namespace basalt::server::runtime::player
