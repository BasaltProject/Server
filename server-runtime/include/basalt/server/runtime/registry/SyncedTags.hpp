#pragma once

#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealFluidTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealTimelineTags.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {
class RealTimelines;
}

namespace basalt::server::runtime::registry {

struct TagSources {
    const world::realBlockTags::TagMembers* blocks = nullptr;
    const world::realItemTags::TagMembers* items = nullptr;
    const world::realFluidTags::TagMembers* fluids = nullptr;
    const world::realEntityTypeTags::TagMembers* entityTypes = nullptr;
    const world::realTimelineTags::TagMembers* timelines = nullptr;
    const world::RealTimelines* timelineOrder = nullptr;
};

[[nodiscard]] std::vector<wire::protocol::configuration::TagGroup> syncedTags(const TagSources& sources);

inline constexpr std::int32_t kPlayTagsId = 0x89;

} // namespace basalt::server::runtime::registry
