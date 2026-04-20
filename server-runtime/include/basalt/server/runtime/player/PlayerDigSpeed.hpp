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

[[nodiscard]] float destroyProgressPerTickFor(const PlayerSession& session, const wire::common::Identifier& block,
                                              const registry::BlockHardness* hardness,
                                              const world::realBlockTags::TagMembers* blockTags, bool underwater);

} // namespace basalt::server::runtime::player
