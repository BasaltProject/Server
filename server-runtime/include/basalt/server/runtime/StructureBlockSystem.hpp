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

class StructureBlockSystem {
public:
    StructureBlockSystem(world::StructureTemplates& templates, player::PlayerRegistry& players);

    [[nodiscard]] std::optional<std::int32_t> handle(world::LoadedWorld& world, player::PlayerSession& player,
                                                     const wire::protocol::play::SetStructureBlockPacket& packet);

    void trigger(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

private:
    bool save(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at, bool toDisk);

    bool detectSize(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    bool place(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at, bool sameSizeOnly);

    void sendBlockEntity(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    world::StructureTemplates* templates_;
    player::PlayerRegistry* players_;
};

} // namespace basalt::server::runtime
