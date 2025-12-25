#pragma once

#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// Metadata needed to render immutable entities emitted by structure templates.
[[nodiscard]] std::vector<wire::protocol::play::SetEntityDataPacket::Entry>
generatedEntityMetadata(std::string_view type, const wire::nbt::NbtCompound& data);

/// One piece of gear a generated entity was written wearing.
struct GeneratedEquipment {
    wire::protocol::play::SetEquipmentPacket::SlotType slot =
        wire::protocol::play::SetEquipmentPacket::SlotType::MainHand;
    std::string item;
    std::int32_t count = 0;
    wire::nbt::NbtCompound components;
};

[[nodiscard]] std::vector<GeneratedEquipment> generatedEntityEquipment(const wire::nbt::NbtCompound& data);

/// The live synchronized fields this server mutates after an entity has spawned.
struct RuntimeEntityMetadata {
    std::int32_t airSupply = 0;
    std::optional<bool> baby;
    std::optional<std::int32_t> cubeSize;
    std::optional<std::int8_t> horseFlags;

    /// Initial non-default fields, or every field whose live value changed since `previous`.
    [[nodiscard]] std::vector<wire::protocol::play::SetEntityDataPacket::Entry>
    changesFrom(const RuntimeEntityMetadata* previous) const;

    friend bool operator==(const RuntimeEntityMetadata&, const RuntimeEntityMetadata&) = default;
};

[[nodiscard]] RuntimeEntityMetadata runtimeEntityMetadata(const ServerEntity& entity);

} // namespace basalt::server::runtime::entity
