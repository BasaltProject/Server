#pragma once

#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace basalt::server::runtime {

class ClientSession;

/// What this client has been told to draw: other players, plugin NPCs, dropped items, summoned
/// entities and generated-chunk entities, each spawned once and only re-sent on a real change.
class EntityVisibilityTracker {
public:
    explicit EntityVisibilityTracker(ClientSession& owner);

    /// The entities the world holds, spawned for this client and moved as they move.
    void syncSummonedEntities();

    void syncPlayers();

    void syncNpcs();

    void syncItemEntities();

    void syncGeneratedEntities();

    /// Everything drawn is forgotten, so the next sync sends it again. What a respawn and a world
    /// change both need: the client throws its whole level away when it handles either.
    void forgetEverythingDrawn();

private:
    ClientSession* owner_;
    /// Which entities this client has been sent, and what was last said about each.
    std::set<std::int32_t> drawnEntities_;
    std::map<std::int32_t, entity::RuntimeEntityMetadata> drawnMetadata_;
    std::set<wire::common::Uuid> drawnPlayers_;
    std::set<std::int32_t> drawnItems_;
    std::set<std::string> drawnNpcs_;
};

} // namespace basalt::server::runtime
