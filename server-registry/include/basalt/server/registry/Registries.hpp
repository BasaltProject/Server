#pragma once

#include <basalt/server/registry/Registry.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::registry {
struct Item {
    wire::common::Identifier id;
    std::int32_t protocolId = 0;

    friend bool operator==(const Item&, const Item&) = default;
};

namespace items {
[[nodiscard]] const Registry<Item>& registry();
} // namespace items

struct EntityTypeEntry {
    wire::common::Identifier id;
    std::int32_t protocolId = 0;

    friend bool operator==(const EntityTypeEntry&, const EntityTypeEntry&) = default;
};

namespace entityTypes {
[[nodiscard]] std::span<const EntityTypeEntry> all();

[[nodiscard]] const EntityTypeEntry* of(const wire::common::Identifier& id);

[[nodiscard]] const EntityTypeEntry* byProtocolId(std::int32_t protocolId);

// parity: `minecraft:player` cannot be summoned, and vanilla refuses it by name.
[[nodiscard]] const wire::common::Identifier& player();
} // namespace entityTypes

struct ParticleType {
    wire::common::Identifier id;
    std::int32_t protocolId = 0;
    wire::protocol::play::ParticleOptionShape optionShape = wire::protocol::play::ParticleOptionShape::None;

    [[nodiscard]] bool needsOptions() const noexcept {
        return optionShape != wire::protocol::play::ParticleOptionShape::None;
    }
};

// parity: the shapes come from `ParticleTypes`' own registrations in the jar.
namespace particleTypes {
[[nodiscard]] std::span<const ParticleType> all();

[[nodiscard]] const ParticleType* of(const wire::common::Identifier& id);
} // namespace particleTypes

namespace dataComponents {
[[nodiscard]] std::optional<std::int32_t> idOf(const wire::common::Identifier& name);

[[nodiscard]] const wire::common::Identifier* nameOf(std::int32_t id);

[[nodiscard]] const std::vector<wire::common::Identifier>& all();
} // namespace dataComponents

namespace blockEntityTypes {
[[nodiscard]] std::optional<std::int32_t> idOf(const wire::common::Identifier& identifier);

// parity: `BlockEntityType.isValid`.
[[nodiscard]] const wire::common::Identifier* ofBlock(const wire::common::Identifier& block);

[[nodiscard]] std::size_t size();
} // namespace blockEntityTypes
} // namespace basalt::server::registry
