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

/// The loaded tags a client is told about, each null where nothing was loaded.
struct TagSources {
    const world::realBlockTags::TagMembers* blocks = nullptr;
    const world::realItemTags::TagMembers* items = nullptr;
    const world::realFluidTags::TagMembers* fluids = nullptr;
    const world::realEntityTypeTags::TagMembers* entityTypes = nullptr;
    const world::realTimelineTags::TagMembers* timelines = nullptr;
    /// The timeline registry as sent, which is what a timeline tag's numbers index.
    const world::RealTimelines* timelineOrder = nullptr;
};

/// parity: `TagNetworkSerialization.serializeTagsToNetwork`: every tag of the synchronised
/// registries, members as network ids. The names vanilla ships go out even when a registry's
/// members are not loaded here, and every tag a data pack adds goes out beside them.
[[nodiscard]] std::vector<wire::protocol::configuration::TagGroup> syncedTags(const TagSources& sources);

/// The play-state id of the same packet. parity: `ClientboundUpdateTagsPacket`, what `/reload` sends.
inline constexpr std::int32_t kPlayTagsId = 0x89;

} // namespace basalt::server::runtime::registry
