#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
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

/// Declared, not included: a world is what `switchWorld` moves someone into, and the connection
/// only ever holds a reference to one.
namespace basalt::server::runtime::world {
class LoadedWorld;
} // namespace basalt::server::runtime::world

/// The same, for one dialog already flattened to what a screen of buttons can draw.
namespace basalt::server::runtime::world::dialogs {
struct Dialog;
} // namespace basalt::server::runtime::world::dialogs

namespace basalt::server::runtime::player {

/// One piece of the title screen. parity: the other door sends five packets for these and this
/// edition one kind with a type on it, so what travels is which piece it is.
enum class TitlePiece : std::uint8_t {
    Title,
    Subtitle,
    ActionBar,
    Times,
    /// Takes what is on screen away; `Reset` also puts the times back to their defaults.
    Clear,
    Reset,
};

/// What a player's session needs of the connection they arrived on.
///
/// Five facts and one call, handed over rather than reached for: the session is what outlives a
/// packet exchange, and a connection is only how it reaches the client. Keeping the seam this
/// narrow is what lets the session, and everything built on it, be written and tested without the
/// networking around it.
struct PlayerConnection {
    /// What the client said in its handshake, and where it connected from: the same facts a ping
    /// carries.
    std::int32_t protocolVersion = 0;
    std::string connectedHost;
    std::optional<std::string> address;

    /// Enchantment name to registry index, or unset until the enchantment registry has gone out.
    item::itemComponentCodecs::EnchantmentId enchantmentIds;

    /// Sends a system message to this player. Unset, nothing is sent, which is what a session with
    /// no connection behind it does.
    std::function<void(const wire::text::Component&)> sendSystemMessage;

    /// Writes one play packet to this player. Unset, nothing is sent.
    ///
    /// The packet writes itself into the buffer the connection hands over, so nothing here needs to
    /// know what any of them look like.
    std::function<void(std::int32_t packetId, const std::function<void(wire::io::ByteBuffer&)>& write)> sendPlayPacket;

    /// What a block that changed asks everyone in this player's world to hear: a door, a note, a
    /// bell. One connection sends these to its whole world rather than only to itself.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                       const std::vector<world::BlockEffect>& effects)>
        broadcastEffects;

    /// What a block told this player alone to do with it: the lid of a chest, the note of a note
    /// block, the shove of a piston. parity: `Level.blockEvent`, whose two numbers mean whatever
    /// the block says they mean, and which both editions read the same way.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, std::int32_t b0,
                       std::int32_t b1)>
        sendBlockEvent;

    /// One of the numbered one-off effects, to this player alone. parity: `Level.levelEvent`.
    ///
    /// The numbers are Java's, and Bedrock's are its own: a connection that cannot translate one
    /// leaves this unset rather than drawing the wrong thing.
    std::function<void(std::int32_t type, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t data)>
        sendLevelEvent;

    /// A boss bar this player should see, by the id whoever raised it knows it under. An empty
    /// title with a progress of zero takes it away again.
    std::function<void(const wire::common::Uuid& id, const wire::text::Component& title, float progress, bool show)>
        showBossBar;

    /// How hard the world is now. Both editions draw it in their own menu and neither reads the
    /// other's packet for it.
    std::function<void(std::int32_t difficulty, bool locked)> setDifficulty;

    /// Who is riding what. Java says it as the whole list of passengers a vehicle carries; the
    /// other edition says it one rider at a time, so the caller hands over the list and each
    /// connection says it its own way. An empty list takes everybody off.
    std::function<void(std::int32_t vehicleId, const std::vector<std::int32_t>& riders)> sendPassengers;

    /// Opens the editor over a sign this player just put down. Java asks for it with a packet of
    /// its own and so does the other edition, but neither reaches the other's client.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> openSignEditor;

    /// A sign whose text changed, with both faces as plain lines. Java carries the whole block
    /// entity in one shape and the other edition in another, so the caller hands over the text and
    /// each connection says it the way its client reads it.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& front,
                       const std::string& back, bool waxed)>
        sendSignText;

    /// Sends this player to another server, host and port. A connection that cannot be moved
    /// leaves this unset and the command says so rather than pretending it worked.
    std::function<void(const std::string& host, std::int32_t port)> transferTo;

    /// Somebody else's arm swinging, which is what makes a hit look like a hit. The client that
    /// swung animates itself, so this only ever carries somebody else's entity.
    std::function<void(std::int32_t entityId, bool offhand)> sendSwing;

    /// The stars over something that just took a critical hit.
    std::function<void(std::int32_t entityId)> showCriticalHit;

    /// A stack flying into whoever picked it up, by the two entities involved.
    std::function<void(std::int32_t itemEntityId, std::int32_t collectorEntityId)> sendPickupAnimation;

    /// The flash and the bang of an explosion. Java has a packet of its own for it and does not
    /// set this; the other edition draws it off a numbered effect, so it is asked separately
    /// rather than translated from a packet that has no counterpart.
    std::function<void(double x, double y, double z)> sendExplosion;

    /// One block that changed, to this player alone. The caller decides who is watching; this only
    /// knows how to say it on the edition this connection speaks.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)> sendBlockChange;

    /// A map's colours and markers in Java's shape. Unset on Java, which takes the packet itself; the
    /// other edition draws maps its own way.
    std::function<void(const wire::protocol::play::MapItemDataPacket& map)> sendMapData;
    /// Plays a sound where this player can hear it, named the way Java names it. A connection that
    /// speaks another edition translates the name itself.
    std::function<void(std::string_view sound, double x, double y, double z, float volume, float pitch)> playSound;

    /// Shows a particle where this player can see it, named the way Java names it. A connection
    /// that speaks another edition translates the name itself, and drops what it cannot name.
    std::function<void(std::string_view particle, double x, double y, double z)> showParticle;

    /// Draws somebody being hurt: the flash, and whatever else that edition paints on a hit. The
    /// damage type is the Java registry id, which an edition without that table ignores.
    std::function<void(std::int32_t entityId, std::int32_t damageTypeId, std::int32_t causeEntityId)> showDamage;

    /// Shoves this player, in blocks per tick, away from the thing at that angle. The angle is
    /// where the hit came from, which is what turns the red on the screen towards it.
    std::function<void(double velocityX, double velocityY, double velocityZ, float fromYaw)> pushBack;

    /// Redraws the yellow half of the hearts. Its own callback because an edition may draw
    /// absorption from somewhere other than where it draws health.
    std::function<void()> syncAbsorption;

    /// Sends one chunk again, for a change too big to patch block by block.
    std::function<void(std::int32_t chunkX, std::int32_t chunkZ)> resendChunk;

    /// Sends new biomes for these chunk columns without resending the columns themselves.
    std::function<void(const std::vector<std::pair<std::int32_t, std::int32_t>>& columns)> sendBiomes;

    /// Pushes the world's weather, which is three game events rather than one packet.
    std::function<void(const world::WorldWeather& weather)> sendWeather;

    /// A line over the hotbar. Its own callback rather than a packet, because what draws it is not
    /// the same thing on every edition.
    std::function<void(const wire::text::Component& message)> sendActionBar;

    /// A title, its subtitle and how long the three parts of it last.
    std::function<void(const wire::text::Component& title, const wire::text::Component& subtitle, std::int32_t fadeIn,
                       std::int32_t stay, std::int32_t fadeOut)>
        sendTitle;

    /// Closes this connection with a reason the client shows. Unset, nothing happens.
    std::function<void(const wire::text::Component& reason)> disconnect;

    /// Puts this player in a game mode, which is a packet and a change on the session at once.
    std::function<void(wire::protocol::play::Gamemode mode)> setGamemode;

    /// Re-sends every inventory slot, which is how a change made on the server side is believed:
    /// the client draws from its own copy and never asks.
    std::function<void()> sendInventoryAll;

    /// Moves the hand of a player who did not move it themselves, after a pick or a container.
    std::function<void(std::int32_t slot)> setHeldSlot;

    /// Gives health back, up to whatever their maximum is.
    std::function<void(float amount)> heal;

    /// Damage that goes through the ordinary path: armour, absorption, the death message and the
    /// hurt animation. The message id is what a death sentence is looked up by, the type is what
    /// the tags are tested against.
    ///
    /// The source travels with it because only the caller knows it: below there is an id and no way
    /// to name whoever swung.
    std::function<void(float amount, std::string_view deathMessageId, const wire::common::Identifier& damageType,
                       const std::optional<entity::DamageSource>& source)>
        hurt;

    /// Tells the client an effect started, or that it is over. The client draws its own icon and
    /// screen tint from these and from nothing else.
    /// Starts an effect on this player, which is what a splash potion and a command both do.
    std::function<void(const wire::common::Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier)>
        addEffect;

    /// Shows the world border to a client that has none of its own: the wall the other edition's
    /// client draws becomes a patch of refusal particles, and the surface it collides with becomes
    /// something invisible standing in the way. `withWall` is only the particles, which are worth
    /// once a second rather than once a turn.
    std::function<void(double minX, double maxX, double minZ, double maxZ, std::int32_t warningBlocks, bool withWall)>
        showBorder;

    /// A team being made, changed or unmade, and who is in it. parity:
    /// `ClientboundSetPlayerTeamPacket`, which only the edition that has teams is told.
    std::function<void(const std::string& team, std::uint8_t method,
                       const std::optional<wire::protocol::play::SetPlayerTeamPacket::Parameters>& parameters,
                       const std::vector<std::string>& members)>
        sendPlayerTeam;

    /// What somebody else's name reads as now, with their team's colour and affixes already in it.
    /// The edition that has no teams is told the finished line instead of the team.
    std::function<void(std::int32_t entityId, const wire::text::Component& nameTag)> sendNameTag;

    /// A scoreboard objective being made, renamed or unmade, and which slot shows it. parity:
    /// `ClientboundSetObjectivePacket` and `ClientboundSetDisplayObjectivePacket`, split this way
    /// because the other edition has no packet for an objective nobody is shown.
    std::function<void(const std::string& objective, std::uint8_t method,
                       const std::optional<wire::text::Component>& title)>
        sendObjective;
    std::function<void(wire::protocol::play::DisplaySlot slot, const std::string& objective)> sendDisplayObjective;

    /// One score, and the same score taken away. An objective nobody named takes every score that
    /// holder has, which is what the other door's own packet means by leaving it out.
    std::function<void(const std::string& holder, const std::string& objective, std::int32_t value)> sendScore;
    std::function<void(const std::string& holder, const std::optional<std::string>& objective)> sendScoreRemoved;

    /// Where the world's spawn moved to, which is what a compass points at. The world travels with
    /// it because each door names the place differently: one by its level key, the other by a
    /// number for its dimension.
    std::function<void(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, float angle)>
        sendSpawnPosition;

    /// Silences a sound, or every sound when no name is given. The mixer is the other door's own
    /// idea: this edition cannot pick one, so a stop that only names a mixer stops everything.
    std::function<void(const std::optional<std::string>& sound,
                       const std::optional<wire::protocol::play::SoundSource>& source)>
        stopSound;

    /// Greys an item out for as long as its cooldown lasts.
    std::function<void(const wire::common::Identifier& item, std::int32_t durationTicks)> startItemCooldown;

    /// One piece of the title screen, which is what `/title` sends. The whole-screen `sendTitle`
    /// above is what a plugin asks for instead: it says everything at once.
    std::function<void(TitlePiece piece, const wire::text::Component& text, std::int32_t fadeIn, std::int32_t stay,
                       std::int32_t fadeOut)>
        sendTitlePiece;

    /// Whose eyes they watch through. Their own entity id puts the camera back, which is how a
    /// spectator stops. The edition with no spectating of its own moves a camera by hand instead.
    std::function<void(std::int32_t entityId)> setCamera;

    /// One dialog to put in front of them. The other door only needs its registry id, since the
    /// client draws it from the registry it was sent; this one has no dialogs of its own and is
    /// handed the whole thing to build a form out of.
    std::function<void(std::int32_t dialogId, const world::dialogs::Dialog& dialog)> showDialog;

    /// One entry of the locator bar: which waypoint it is, the entity it follows, where that is,
    /// and the colour of the dot. `style` names the icon the other door draws it with; this one
    /// has a single style and works the icon out from how far away the viewer is. A colour of
    /// none is the icon's own. `tracked` false takes the entry off the bar again.
    std::function<void(const wire::common::Uuid& waypoint, std::int32_t entityId, const std::string& style,
                       const std::optional<std::int32_t>& color, double x, double y, double z, bool tracked)>
        sendWaypoint;

    /// Draws somebody else's cracks on a block. `stage` is the ten steps the other door counts in
    /// and `perTick` how much of the block comes off in a turn, because this edition draws the
    /// animation itself off a pace rather than off a step. A negative stage takes the cracks off.
    std::function<void(std::int32_t entityId, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stage,
                       float perTick)>
        sendBlockDestruction;

    std::function<void(const world::ActiveEffect& effect)> sendEffect;
    std::function<void(const wire::common::Identifier& effect)> sendEffectRemoved;

    /// Moves the player to another world, at the coordinates given. Answers false when the move did
    /// not happen, which is how a command reports that nobody went anywhere.
    std::function<bool(world::LoadedWorld& world, double x, double y, double z)> switchWorld;

    /// Moves the player and tells their client, which is the only way a position set on the server
    /// is believed: a client that was not told rubber-bands straight back.
    std::function<void(double x, double y, double z, float yaw, float pitch)> teleportTo;

    /// Draws somebody lying in a bed, or standing up out of one, on **this** player's screen: the
    /// sleeper is named by entity, so the same call serves the sleeper and everyone watching them.
    std::function<void(std::int32_t entityId, std::optional<std::array<std::int32_t, 3>> bed)> showSleeping;

    /// Gets this player out of the bed they are in, which is a packet and a pose at once.
    std::function<void()> wakeUp;

    /// Draws the merchant's book again on a client that has one open, which is how an offer that
    /// has just been used stops being offered. Unset where a window of its own carries the offers.
    std::function<void()> sendTrades;

    /// Draws the three rows of an open enchanting table. Unset on a door whose screen reads them
    /// out of the window's own numbers instead.
    std::function<void()> sendEnchantOptions;

    /// Draws somebody's emote on this player's screen. Unset on a door whose clients have none, so
    /// an emote reaches only the clients that can draw one.
    std::function<void(std::int32_t entityId, const std::string& emoteId, std::int32_t duration, std::uint8_t flags)>
        showEmote;

    /// Pushes the world's clock, which the client runs on its own between updates.
    std::function<void(const world::LoadedWorld& world)> sendTime;

    /// Pushes the air supply and the burning flag, which the HUD draws its bubbles and flames from.
    std::function<void()> syncSubmersion;

    /// Re-sends the command tree, which is filtered by permission and so goes stale the moment a
    /// player's group changes.
    std::function<void()> sendCommandTree;

    /// Pushes this player's level and bar after something moved them, since the client draws its
    /// own copy and never asks.
    std::function<void()> syncExperience;

    /// Pushes health, hunger and absorption, which the HUD draws its hearts and haunches from.
    std::function<void()> syncHealth;

    std::function<void(std::string_view deathMessageId)> killPlayer;

    /// The screen the dying player is shown, with the sentence on it. The only part of dying that
    /// is their edition's own business, which is why it is here and not in the death itself.
    std::function<void(const wire::text::Component& message)> sendCombatDeath;

    /// A window a plugin drew, which is the session's business: the container machinery and the
    /// socket are both already there.
    std::function<void(const std::string& title, std::int32_t rows,
                       const std::map<std::int32_t, plugins::PluginItem>& items,
                       std::function<void(std::int32_t)> onClick)>
        openPluginMenu;

    std::function<void(const std::map<std::int32_t, plugins::PluginItem>& items)> refreshPluginMenu;

    std::function<void()> closeOpenMenu;

    /// A turn of the world for whatever window this player has open: the numbers a furnace draws
    /// its fire and its arrow from. Called on the tick thread, between turns, like the hand-over
    /// below. Unset for a door whose own loop already keeps its windows up to date.
    std::function<void(world::LoadedWorld& world)> syncOpenWindow;

    /// Moves this session onto the owner of the world it is standing in, between ticks.
    ///
    /// Called from the global phase, where no region is running: handing a session that walked
    /// across a border to its new owner mid-tick is two threads inside the same player.
    std::function<void(world::LoadedWorld& world)> handOverToRegion;

    /// This player as the database stores them, or nothing when there is nothing worth writing yet.
    ///
    /// The connection owns it because what a saved player is made of (position, inventory, effects)
    /// lives across the whole session, not in any one of the pieces that change it.
    std::function<std::optional<storage::StoredPlayer>()> storedSnapshot;
};

} // namespace basalt::server::runtime::player
