#pragma once

#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime {

class ClientSession;

/// One plugin NPC as this tracker draws it: a player-shaped entity nobody else in the world owns.
///
/// A view rather than the registry itself: the plugin bridge owns those, and this only needs to
/// know where they are and what they look like.
struct NpcView {
    std::int32_t entityId = 0;
    wire::common::Uuid uuid;
    /// Bumped by whoever owns it when anything about it changes, which redraws it.
    std::int32_t version = 0;
    std::string world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    bool looksAtPlayers = false;
    std::optional<std::string> skinTexture;
    std::optional<std::string> skinSignature;
    /// Who may see it, or unset for everybody.
    std::optional<std::vector<std::string>> visibleTo;
};

/// What this client has been told to draw: other players, plugin NPCs, dropped items, summoned
/// entities and generated-chunk entities, each spawned once and only re-sent on a real change.
class EntityVisibilityTracker {
public:
    explicit EntityVisibilityTracker(ClientSession& owner);

    /// Brings this client's view of summoned entities up to date: spawns what it has not seen,
    /// moves what has been moved, and removes what was killed.
    void syncSummonedEntities();

    void syncPlayers();

    void syncNpcs();

    void syncItemEntities();

    void syncGeneratedEntities();

    /// Everything drawn is forgotten, so the next sync sends it again. What a respawn and a world
    /// change both need: the client throws its whole level away when it handles either.
    void forgetEverythingDrawn();

private:
    void syncPassengers(const std::vector<entity::ServerEntity*>& entities, const std::string& viewer,
                        std::int32_t viewerEntityId);

    void syncLeash(const entity::ServerEntity& entity);

    void syncEquipmentOf(const player::PlayerSession& other);

    void sendDisplayText(std::int32_t entityId, const std::string& text);

    /// parity: the team that hides a name tag, which an NPC joins before it is drawn.
    void hideNameTagOf(const std::string& member);

    /// Whether this NPC's head has turned far enough to be worth a packet.
    [[nodiscard]] bool lookedAt(std::int32_t entityId, float yaw, float pitch);

    ClientSession* owner_;
    /// Which dropped items this client has been told about, and with what count.
    std::set<std::int32_t> trackedItems_;
    std::map<std::int32_t, std::int32_t> trackedItemCounts_;
    /// Which of those have already had their final resting position sent.
    std::set<std::int32_t> settledItems_;
    /// Summoned entities this client has been told about, so each is spawned once.
    std::set<std::int32_t> trackedEntities_;
    /// NPCs this client has been told about, and which version of each.
    std::map<std::int32_t, std::int32_t> trackedNpcs_;
    bool namelessTeamMade_ = false;
    std::map<std::int32_t, std::pair<float, float>> lastNpcLook_;
    /// Where each of them was last reported, so a mob standing still costs nothing.
    std::map<std::int32_t, std::array<double, 5>> lastEntityPositions_;
    std::map<std::int32_t, entity::RuntimeEntityMetadata> lastRuntimeEntityMetadata_;
    std::map<std::int32_t, std::vector<std::int32_t>> lastPassengers_;
    std::map<std::int32_t, std::int32_t> lastLeashHolders_;
    std::set<std::int32_t> trackedPlayers_;
    std::map<std::int32_t, std::vector<std::optional<storage::StoredItemStack>>> trackedEquipment_;
    std::set<std::int32_t> trackedGeneratedEntities_;
};

} // namespace basalt::server::runtime
