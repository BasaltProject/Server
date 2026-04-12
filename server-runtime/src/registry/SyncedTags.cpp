#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/registry/SyncedTags.hpp>
#include <basalt/server/runtime/registry/VanillaTagData.hpp>
#include <basalt/server/runtime/world/RealTimelines.hpp>

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

namespace basalt::server::runtime::registry {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

/// A tag's members as the client numbers them. Members are ids of the tagged thing: a block tag
/// carries `minecraft:block` ids, not block state ids.
[[nodiscard]] std::vector<std::int32_t> membersOf(const TagSources& sources, std::string_view registryId,
                                                  const Identifier& tag) {
    std::vector<std::int32_t> ids;
    if (registryId == "minecraft:block" && sources.blocks != nullptr) {
        for (const Identifier& member : world::realBlockTags::membersOf(*sources.blocks, tag.toString())) {
            if (const std::optional<std::int32_t> id = reg::blocks::blockIdOf(member); id.has_value()) {
                ids.push_back(*id);
            }
        }
    } else if (registryId == "minecraft:item" && sources.items != nullptr) {
        for (const Identifier& member : world::realItemTags::membersOf(*sources.items, tag.toString())) {
            if (const reg::Item* item = reg::items::registry().byName(member); item != nullptr) {
                ids.push_back(item->protocolId);
            }
        }
    } else if (registryId == "minecraft:fluid" && sources.fluids != nullptr) {
        const auto& numbers = world::realFluidTags::protocolIds();
        for (const Identifier& member : world::realFluidTags::membersOf(*sources.fluids, tag)) {
            if (const auto found = numbers.find(member); found != numbers.end()) {
                ids.push_back(found->second);
            }
        }
    } else if (registryId == "minecraft:timeline" && sources.timelines != nullptr && sources.timelineOrder != nullptr) {
        const std::vector<Identifier> order = sources.timelineOrder->ids();
        for (const Identifier& member : world::realTimelineTags::membersOf(*sources.timelines, tag)) {
            if (const auto found = std::ranges::find(order, member); found != order.end()) {
                ids.push_back(static_cast<std::int32_t>(std::distance(order.begin(), found)));
            }
        }
    } else if (registryId == "minecraft:entity_type" && sources.entityTypes != nullptr) {
        for (const Identifier& member : world::realEntityTypeTags::membersOf(*sources.entityTypes, tag)) {
            if (const reg::EntityTypeEntry* type = reg::entityTypes::of(member); type != nullptr) {
                ids.push_back(type->protocolId);
            }
        }
    }
    return ids;
}

/// Every tag name a loaded table holds for a registry, so a pack's own tags reach the client too.
template <typename Members>
void namesOf(const Members* tags, std::set<Identifier>& names) {
    if (tags == nullptr) {
        return;
    }
    for (const auto& [id, members] : *tags) {
        names.insert(id);
    }
}

} // namespace

std::vector<wire::protocol::configuration::TagGroup> syncedTags(const TagSources& sources) {
    std::vector<wire::protocol::configuration::TagGroup> groups;
    for (const auto& [registryId, tagNames] : vanillaTagData::tagsByRegistry()) {
        std::set<Identifier> names;
        for (const std::string_view name : tagNames) {
            if (const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(name);
                id.has_value()) {
                names.insert(*id);
            }
        }
        if (registryId == "minecraft:block") {
            namesOf(sources.blocks, names);
        } else if (registryId == "minecraft:item") {
            namesOf(sources.items, names);
        } else if (registryId == "minecraft:fluid") {
            namesOf(sources.fluids, names);
        } else if (registryId == "minecraft:entity_type") {
            namesOf(sources.entityTypes, names);
        } else if (registryId == "minecraft:timeline") {
            namesOf(sources.timelines, names);
        }
        std::vector<wire::protocol::configuration::TagEntry> entries;
        entries.reserve(names.size());
        for (const Identifier& name : names) {
            entries.push_back(wire::protocol::configuration::TagEntry{.name = name.toString(),
                                                                      .ids = membersOf(sources, registryId, name)});
        }
        groups.push_back(wire::protocol::configuration::TagGroup{.registryId = registryId, .tags = std::move(entries)});
    }
    return groups;
}

} // namespace basalt::server::runtime::registry
