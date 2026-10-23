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

namespace basalt::server::world::structure {
class StructureTemplate;
}

namespace basalt::server::runtime {

class StructureBlockSystem {
public:
    struct Placement {
        std::int32_t rotation = 0;
        std::int32_t mirror = 0;
        float integrity = 1.0F;
        std::int64_t seed = 0;
        bool ignoreEntities = false;
        std::optional<std::size_t> palette;
    };

    StructureBlockSystem(world::StructureTemplates& templates, player::PlayerRegistry& players);

    [[nodiscard]] std::optional<std::int32_t> handle(world::LoadedWorld& world, player::PlayerSession& player,
                                                     const wire::protocol::play::SetStructureBlockPacket& packet);

    void trigger(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    /// parity: `StructureTemplate.placeInWorld` over every loaded chunk the template covers.
    void placeTemplate(world::LoadedWorld& world, const server::world::structure::StructureTemplate& placed,
                       const std::array<std::int32_t, 3>& origin, const Placement& how);

private:
    bool save(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at, bool toDisk);

    bool detectSize(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    bool place(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at, bool sameSizeOnly);

    void sendBlockEntity(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    world::StructureTemplates* templates_;
    player::PlayerRegistry* players_;
};

} // namespace basalt::server::runtime
