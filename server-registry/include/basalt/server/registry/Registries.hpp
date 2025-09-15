#pragma once

#include <basalt/server/registry/Registry.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::registry {

/// A single item, identified by its vanilla network protocol id.
struct Item {
    wire::common::Identifier id;
    std::int32_t protocolId = 0;

    friend bool operator==(const Item&, const Item&) = default;
};

/// Every item, straight from the real 26.2 server jar's own data generator: authoritative protocol
/// ids, not decompiled or guessed.
namespace items {

[[nodiscard]] const Registry<Item>& registry();

} // namespace items

/// One entity type: its id and the protocol number the client resolves it by.
struct EntityTypeEntry {
    wire::common::Identifier id;
    std::int32_t protocolId = 0;

    friend bool operator==(const EntityTypeEntry&, const EntityTypeEntry&) = default;
};

/// The `minecraft:entity_type` registry.
///
/// Types only: what an entity *does* is the entity's own business. What this needs from the registry
/// is the number to spawn one with, which is what `/summon` looks up.
namespace entityTypes {

/// In protocol id order.
[[nodiscard]] std::span<const EntityTypeEntry> all();

[[nodiscard]] const EntityTypeEntry* of(const wire::common::Identifier& id);

[[nodiscard]] const EntityTypeEntry* byProtocolId(std::int32_t protocolId);

/// parity: `minecraft:player` cannot be summoned, and vanilla refuses it by name.
[[nodiscard]] const wire::common::Identifier& player();

} // namespace entityTypes

/// One particle type: its id, its protocol number, and the shape of the options it carries.
struct ParticleType {
    wire::common::Identifier id;
    std::int32_t protocolId = 0;
    wire::protocol::play::ParticleOptionShape optionShape = wire::protocol::play::ParticleOptionShape::None;

    [[nodiscard]] bool needsOptions() const noexcept {
        return optionShape != wire::protocol::play::ParticleOptionShape::None;
    }
};

/// The `minecraft:particle_type` registry, plus which option codec each type uses.
///
/// parity: the shapes come from `ParticleTypes`' own registrations in the jar. Getting one wrong is
/// not cosmetic: the options are written straight after the type id with no length, so a type that
/// writes the wrong shape desynchronises the client on the very next packet.
namespace particleTypes {

[[nodiscard]] std::span<const ParticleType> all();

[[nodiscard]] const ParticleType* of(const wire::common::Identifier& id);

} // namespace particleTypes

/// The `minecraft:data_component_type` registry: the numbers an item stack's component patch carries
/// and the names `/give diamond_sword[custom_name=...]` writes.
namespace dataComponents {

[[nodiscard]] std::optional<std::int32_t> idOf(const wire::common::Identifier& name);

[[nodiscard]] const wire::common::Identifier* nameOf(std::int32_t id);

[[nodiscard]] const std::vector<wire::common::Identifier>& all();

} // namespace dataComponents

/// The `minecraft:block_entity_type` registry: which blocks own one, and the numeric id a chunk
/// packet carries for it.
///
/// A block entity type is not always named after its block, and that is not a detail: all eight
/// copper chests are `minecraft:chest`, every shulker box is `minecraft:shulker_box`. Naming one
/// after its block leaves it without a protocol id, so it is dropped from the chunk and the block
/// turns invisible.
namespace blockEntityTypes {

/// The protocol id for `minecraft:<path>`, or absent for a type this version does not register.
[[nodiscard]] std::optional<std::int32_t> idOf(const wire::common::Identifier& identifier);

/// The block entity `block` owns, or null when it owns none. parity: `BlockEntityType.isValid`.
[[nodiscard]] const wire::common::Identifier* ofBlock(const wire::common::Identifier& block);

[[nodiscard]] std::size_t size();

} // namespace blockEntityTypes

} // namespace basalt::server::registry
