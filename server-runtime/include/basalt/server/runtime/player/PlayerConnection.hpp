#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/chat/ChatMessage.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::world::dialogs {
struct Dialog;
} // namespace basalt::server::runtime::world::dialogs

namespace basalt::server::runtime::player {

enum class TitlePiece : std::uint8_t {
    Title,
    Subtitle,
    ActionBar,
    Times,
    Clear,
    Reset,
};

struct PlayerConnection {
    std::int32_t protocolVersion = 0;
    std::string connectedHost;
    std::optional<std::string> address;

    item::itemComponentCodecs::EnchantmentId enchantmentIds;

    std::function<void(const wire::text::Component&)> sendSystemMessage;

    std::function<void(const chat::PlayerChatMessage& message, const chat::BoundChatType& chatType)> sendChatMessage;

    std::function<void(std::int32_t packetId, const std::function<void(wire::io::ByteBuffer&)>& write)> sendPlayPacket;

    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                       const std::vector<world::BlockEffect>& effects)>
        broadcastEffects;

    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, std::int32_t b0,
                       std::int32_t b1)>
        sendBlockEvent;

    std::function<void(std::int32_t type, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t data)>
        sendLevelEvent;

    std::function<void(const wire::common::Uuid& id, const wire::text::Component& title, float progress, bool show)>
        showBossBar;

    std::function<void(std::int32_t difficulty, bool locked)> setDifficulty;

    std::function<void(std::int32_t vehicleId, const std::vector<std::int32_t>& riders)> sendPassengers;

    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> openSignEditor;

    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& front,
                       const std::string& back, bool waxed)>
        sendSignText;

    std::function<void(const std::string& host, std::int32_t port)> transferTo;

    std::function<void(std::int32_t entityId, bool offhand)> sendSwing;

    std::function<void(std::int32_t entityId)> showCriticalHit;

    std::function<void(std::int32_t itemEntityId, std::int32_t collectorEntityId)> sendPickupAnimation;

    std::function<void(double x, double y, double z)> sendExplosion;

    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)> sendBlockChange;

    std::function<void(const wire::protocol::play::MapItemDataPacket& map)> sendMapData;
    std::function<void(std::string_view sound, double x, double y, double z, float volume, float pitch)> playSound;

    std::function<void(std::string_view particle, double x, double y, double z)> showParticle;

    std::function<void(std::int32_t entityId, std::int32_t damageTypeId, std::int32_t causeEntityId)> showDamage;

    std::function<void(double velocityX, double velocityY, double velocityZ, float fromYaw)> pushBack;

    std::function<void()> syncAbsorption;

    std::function<void(std::int32_t chunkX, std::int32_t chunkZ)> resendChunk;

    std::function<void(const std::vector<std::pair<std::int32_t, std::int32_t>>& columns)> sendBiomes;

    std::function<void(const world::WorldWeather& weather)> sendWeather;

    std::function<void(const wire::text::Component& message)> sendActionBar;

    std::function<void(const wire::text::Component& title, const wire::text::Component& subtitle, std::int32_t fadeIn,
                       std::int32_t stay, std::int32_t fadeOut)>
        sendTitle;

    std::function<void(const wire::text::Component& reason)> disconnect;

    std::function<void(wire::protocol::play::Gamemode mode)> setGamemode;

    std::function<void()> sendInventoryAll;

    std::function<void(std::int32_t slot)> setHeldSlot;

    std::function<void(float amount)> heal;

    std::function<void(float amount, std::string_view deathMessageId, const wire::common::Identifier& damageType,
                       const std::optional<entity::DamageSource>& source)>
        hurt;

    std::function<void(const wire::common::Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier)>
        addEffect;

    std::function<void(double minX, double maxX, double minZ, double maxZ, std::int32_t warningBlocks, bool withWall)>
        showBorder;

    std::function<void(const std::string& team, std::uint8_t method,
                       const std::optional<wire::protocol::play::SetPlayerTeamPacket::Parameters>& parameters,
                       const std::vector<std::string>& members)>
        sendPlayerTeam;

    std::function<void(std::int32_t entityId, const wire::text::Component& nameTag)> sendNameTag;

    std::function<void(const std::string& objective, std::uint8_t method,
                       const std::optional<wire::text::Component>& title)>
        sendObjective;
    std::function<void(wire::protocol::play::DisplaySlot slot, const std::string& objective)> sendDisplayObjective;

    std::function<void(const std::string& holder, const std::string& objective, std::int32_t value)> sendScore;
    std::function<void(const std::string& holder, const std::optional<std::string>& objective)> sendScoreRemoved;

    std::function<void(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, float angle)>
        sendSpawnPosition;

    std::function<void(const std::optional<std::string>& sound,
                       const std::optional<wire::protocol::play::SoundSource>& source)>
        stopSound;

    std::function<void(const wire::common::Identifier& item, std::int32_t durationTicks)> startItemCooldown;

    std::function<void(TitlePiece piece, const wire::text::Component& text, std::int32_t fadeIn, std::int32_t stay,
                       std::int32_t fadeOut)>
        sendTitlePiece;

    std::function<void(std::int32_t entityId)> setCamera;

    std::function<void(std::int32_t dialogId, const world::dialogs::Dialog& dialog)> showDialog;

    /// A plugin's own form: shown at once, and answered through the dialog's `submit`.
    std::function<void(const world::dialogs::Dialog& dialog)> showForm;

    std::function<void(const wire::common::Uuid& waypoint, std::int32_t entityId, const std::string& style,
                       const std::optional<std::int32_t>& color, double x, double y, double z, bool tracked)>
        sendWaypoint;

    std::function<void(std::int32_t entityId, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stage,
                       float perTick)>
        sendBlockDestruction;

    std::function<void(const world::ActiveEffect& effect)> sendEffect;
    std::function<void(const wire::common::Identifier& effect)> sendEffectRemoved;

    std::function<bool(world::LoadedWorld& world, double x, double y, double z)> switchWorld;

    std::function<void(double x, double y, double z, float yaw, float pitch)> teleportTo;

    std::function<void()> dismount;

    std::function<void(std::int32_t entityId, std::optional<std::array<std::int32_t, 3>> bed)> showSleeping;

    std::function<void()> wakeUp;

    std::function<void()> sendTrades;

    std::function<void()> sendEnchantOptions;

    std::function<void(std::int32_t entityId, const std::string& emoteId, std::int32_t duration, std::uint8_t flags)>
        showEmote;

    std::function<void(const world::LoadedWorld& world)> sendTime;

    std::function<void()> syncSubmersion;

    std::function<void()> sendCommandTree;

    std::function<void()> syncExperience;

    std::function<void()> syncHealth;

    std::function<void(std::string_view deathMessageId)> killPlayer;

    std::function<void(const wire::text::Component& message)> sendCombatDeath;

    std::function<void(const std::string& title, std::int32_t rows,
                       const std::map<std::int32_t, server::plugins::PluginItem>& items,
                       std::function<void(std::int32_t)> onClick)>
        openPluginMenu;

    std::function<void(const std::map<std::int32_t, server::plugins::PluginItem>& items)> refreshPluginMenu;

    std::function<void()> closeOpenMenu;

    std::function<void(world::LoadedWorld& world)> syncOpenWindow;

    /// Moves this session onto the owner of the world it is standing in, between ticks.
    std::function<void(world::LoadedWorld& world)> handOverToRegion;

    std::function<std::optional<storage::StoredPlayer>()> storedSnapshot;
};

} // namespace basalt::server::runtime::player
