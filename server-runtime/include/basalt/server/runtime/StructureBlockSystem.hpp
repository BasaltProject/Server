#pragma once

#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class StructureTemplates;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

/// What a structure block does: its screen's settings, saving and loading a structure, measuring the
/// corners, and the same three off a redstone signal.
///
/// parity: `ServerGamePacketListenerImpl.handleSetStructureBlock`, `StructureBlockEntity` and
/// `StructureBlock.trigger`.
class StructureBlockSystem {
public:
    StructureBlockSystem(world::StructureTemplates& templates, player::PlayerRegistry& players);

    /// parity: `handleSetStructureBlock` for a player allowed to: the settings stored, the button
    /// acted on, and the message it answers with. Answers the block's new state when its mode moved.
    [[nodiscard]] std::optional<std::int32_t> handle(world::LoadedWorld& world, player::PlayerSession& player,
                                                     const wire::protocol::play::SetStructureBlockPacket& packet);

    /// parity: `StructureBlock.trigger`: a save kept in memory, a load placed, a corner forgotten.
    void trigger(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

private:
    /// parity: `saveStructure`, into the templates and to disk when asked.
    bool save(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at, bool toDisk);

    /// parity: `detectSize`.
    bool detectSize(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    /// parity: `placeStructureIfSameSize` when `sameSizeOnly`, `placeStructure` otherwise. Answers
    /// whether it placed; a different size only takes the template's size in.
    bool place(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at, bool sameSizeOnly);

    /// parity: `sendBlockUpdated`, the block entity's data to everybody in the world.
    void sendBlockEntity(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    world::StructureTemplates* templates_;
    player::PlayerRegistry* players_;
};

} // namespace basalt::server::runtime
