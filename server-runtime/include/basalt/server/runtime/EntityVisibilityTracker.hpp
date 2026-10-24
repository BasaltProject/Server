#pragma once

#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/player/PlayerEntityData.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
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

struct NpcView {
    std::int32_t entityId = 0;
    wire::common::Uuid uuid;
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
    std::optional<std::vector<std::string>> visibleTo;
};

class EntityVisibilityTracker {
public:
    explicit EntityVisibilityTracker(ClientSession& owner);

    void syncSummonedEntities();

    void syncPlayers();

    void syncNpcs();

    void syncItemEntities();

    void syncGeneratedEntities();

    void forgetEverythingDrawn();

    [[nodiscard]] bool isTracking(std::int32_t entityId) const {
        return trackedEntities_.contains(entityId) || trackedPlayers_.contains(entityId);
    }

private:
    void syncPassengers(const std::vector<entity::ServerEntity*>& entities, const std::string& viewer,
                        std::int32_t viewerEntityId);

    void syncLeash(const entity::ServerEntity& entity);

    void syncMobEquipment(const entity::ServerEntity& entity);

    void syncEquipmentOf(const player::PlayerSession& other);

    void syncPlayerData(const player::PlayerSession& player);

    void sendDisplayText(std::int32_t entityId, const std::string& text);

    /// parity: the team that hides a name tag, which an NPC joins before it is drawn.
    void hideNameTagOf(const std::string& member);

    [[nodiscard]] bool lookedAt(std::int32_t entityId, float yaw, float pitch);

    ClientSession* owner_;
    std::set<std::int32_t> trackedItems_;
    std::map<std::int32_t, std::int32_t> trackedItemCounts_;
    std::set<std::int32_t> settledItems_;
    std::set<std::int32_t> trackedEntities_;
    std::map<std::int32_t, std::int32_t> trackedNpcs_;
    bool namelessTeamMade_ = false;
    std::map<std::int32_t, std::pair<float, float>> lastNpcLook_;
    std::map<std::int32_t, std::array<double, 5>> lastEntityPositions_;
    std::map<std::int32_t, entity::RuntimeEntityMetadata> lastRuntimeEntityMetadata_;
    std::map<std::int32_t, std::uint32_t> sentSourceVersion_;
    std::map<std::int32_t, std::vector<std::int32_t>> lastPassengers_;
    std::map<std::int32_t, std::int32_t> lastLeashHolders_;
    std::map<std::int32_t, std::vector<entity::GeneratedEquipment>> lastMobEquipment_;
    std::map<std::int32_t, std::string> lastDisplayText_;
    std::set<std::int32_t> trackedPlayers_;
    std::map<std::int32_t, std::int64_t> profileSeen_;
    std::map<std::int32_t, player::PlayerEntityData> lastPlayerData_;
    std::map<std::int32_t, std::vector<std::optional<storage::StoredItemStack>>> trackedEquipment_;
    std::set<std::int32_t> trackedGeneratedEntities_;
};

} // namespace basalt::server::runtime
