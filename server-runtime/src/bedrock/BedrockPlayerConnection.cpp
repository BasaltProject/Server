#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/bedrock/BedrockBlockEntities.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayerInternals.hpp>
#include <basalt/server/runtime/bedrock/BedrockRecords.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/wire/bedrock/BedrockBlocks.hpp>
#include <basalt/wire/bedrock/BedrockEffects.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockParticles.hpp>
#include <basalt/wire/bedrock/BedrockSounds.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::bedrock {

namespace {

/// parity: a waypoint with no colour of its own is drawn in one taken from its uuid, exactly as
/// `UUID.hashCode` builds it and with the top byte thrown away.
[[nodiscard]] std::int32_t waypointColourOf(const wire::common::Uuid& uuid) {
    const std::uint64_t folded = uuid.high() ^ uuid.low();
    const auto hashed = static_cast<std::uint32_t>(folded ^ (folded >> 32U));
    return static_cast<std::int32_t>(hashed & 0xFFFFFFU);
}

/// parity: `Attributes.MOVEMENT_SPEED`'s default for a player, which is what vanilla sends.
constexpr float kWalkSpeed = 0.1F;

/// What this connection knows about the scoreboard it has drawn.
///
/// The other edition names nothing: a line is a number it was given, so the numbers have to be kept
/// to take a line away again. One per connection, because the ids are the client's own.
struct DrawnScoreboard {
    /// The title each objective was made with, which its slot needs when it is put on screen.
    std::map<std::string, std::string, std::less<>> titles;
    /// Which objective each slot shows.
    std::map<std::string, std::string, std::less<>> slots;
    /// The line each holder was given, by objective.
    std::map<std::string, std::map<std::string, std::int64_t, std::less<>>, std::less<>> lines;
    std::int64_t nextLine = 1;
};

/// parity: the three slots the other door names, under the words this one uses.
[[nodiscard]] std::string_view slotName(const wire::protocol::play::DisplaySlot slot) {
    switch (slot) {
    case wire::protocol::play::DisplaySlot::List:
        return "list";
    case wire::protocol::play::DisplaySlot::BelowName:
        return "belowname";
    case wire::protocol::play::DisplaySlot::Sidebar:
        break;
    }
    return "sidebar";
}

} // namespace

namespace {

/// What a client will take as the line under a command in its suggestion list. Since 1.21.130 it
/// refuses a description past a thousand characters, and it refuses the whole packet rather than
/// the one command: a `gamerule` usage naming every rule is nearly three thousand on its own, and
/// it took the command tree, the join and the player with it. A line break is cut for the same
/// reason Geyser cuts one, which is that the list is unreadable after it.
[[nodiscard]] std::string fitsInACommandTip(std::string usage) {
    constexpr std::size_t kMostCharacters = 950;
    if (const std::size_t breaksAt = usage.find('\n'); breaksAt != std::string::npos) {
        usage.resize(breaksAt);
    }
    if (usage.size() > kMostCharacters) {
        usage.resize(kMostCharacters - 3);
        usage += "...";
    }
    return usage;
}

using wire::text::Component;

/// How many turns of the world go by between two readings of an open window's contents. Half a
/// second: long enough that a chest costs nothing, short enough that nobody sees the wait.
constexpr std::int32_t kWindowReadTurns = 10;

} // namespace

/// Everything this player can be told, in the only terms a Bedrock client understands.
///
/// Its own file because it is the whole of that translation and nothing else: the lifecycle, the
/// world around the player and what the client says back stay in `BedrockPlayer.cpp`.
player::PlayerConnection BedrockPlayer::buildConnection(const BedrockListener::Joining& joining) {
    player::PlayerConnection connection;
    // Bedrock has its own numbering, and nothing may read this as a Java protocol version. Zero is
    // what the rest of the server already treats as "no version was announced".
    connection.protocolVersion = 0;
    connection.connectedHost = joining.address;
    connection.address = joining.address;
    // The only thing this connection can do so far. Everything else is unset, and every caller
    // already checks before it sends: a Bedrock player is simply told nothing it cannot be told.
    connection.sendSystemMessage = [mailbox = joining.mailbox, lang = context_.lang](const Component& message) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeText(body, wire::bedrock::TextType::Raw, "",
                                 wire::text::toLegacyText(wire::text::resolveComponent(message, lang)));
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Text), body.toVector());
    };

    connection.disconnect = [mailbox = joining.mailbox, lang = context_.lang](const Component& reason) {
        wire::io::ByteBuffer body;
        // Plain, not legacy: the screen a client draws when it is turned away prints the section
        // signs rather than acting on them, so the colour arrives as litter in front of the words.
        wire::bedrock::writeDisconnect(body, wire::text::toPlainText(wire::text::resolveComponent(reason, lang)));
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Disconnect), body.toVector(), true);
    };
    // The scoreboard, which the other edition draws out of different packets: it has none for an
    // objective nobody is shown, and its lines are numbered rather than named.
    const auto board = std::make_shared<DrawnScoreboard>();
    const auto showObjective = [mailbox = joining.mailbox, board](const std::string& slot,
                                                                  const std::string& objective) {
        wire::io::ByteBuffer body;
        const auto title = board->titles.find(objective);
        wire::bedrock::writeSetDisplayObjective(body, slot, objective,
                                                title == board->titles.end() ? objective : title->second);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetDisplayObjective), body.toVector());
    };
    connection.sendObjective = [mailbox = joining.mailbox, board, lang = context_.lang,
                                showObjective](const std::string& objective, const std::uint8_t method,
                                               const std::optional<Component>& title) {
        if (method == wire::protocol::play::SetObjectivePacket::kMethodRemove) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeRemoveObjective(body, objective);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::RemoveObjective), body.toVector());
            board->titles.erase(objective);
            board->lines.erase(objective);
            std::erase_if(board->slots, [&objective](const auto& shown) { return shown.second == objective; });
            return;
        }
        board->titles[objective] =
            title.has_value() ? wire::text::toLegacyText(wire::text::resolveComponent(*title, lang)) : objective;
        // A title that changed is only worth sending again where it is already on screen.
        for (const auto& [slot, shown] : board->slots) {
            if (shown == objective) {
                showObjective(slot, objective);
            }
        }
    };
    connection.sendDisplayObjective = [mailbox = joining.mailbox, board, showObjective, protocol = joining.protocol](
                                          const wire::protocol::play::DisplaySlot slot, const std::string& objective) {
        const std::string where{slotName(slot)};
        if (objective.empty()) {
            const auto shown = board->slots.find(where);
            if (shown != board->slots.end()) {
                wire::io::ByteBuffer body;
                wire::bedrock::writeRemoveObjective(body, shown->second);
                mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::RemoveObjective),
                              body.toVector());
                board->slots.erase(shown);
            }
            return;
        }
        board->slots[where] = objective;
        showObjective(where, objective);
        // Every score it already holds, because a client is told them only once its objective is up.
        const auto held = board->lines.find(objective);
        if (held == board->lines.end()) {
            return;
        }
        std::vector<wire::bedrock::BedrockScoreLine> lines;
        lines.reserve(held->second.size());
        for (const auto& [holder, lineId] : held->second) {
            lines.push_back({.lineId = lineId, .name = holder, .score = 0});
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetScore(body, objective, lines, protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetScore), body.toVector());
    };
    connection.sendScore = [mailbox = joining.mailbox, board, protocol = joining.protocol](
                               const std::string& holder, const std::string& objective, const std::int32_t value) {
        std::int64_t& lineId = board->lines[objective][holder];
        if (lineId == 0) {
            lineId = board->nextLine++;
        }
        const std::array<wire::bedrock::BedrockScoreLine, 1> line{
            wire::bedrock::BedrockScoreLine{.lineId = lineId, .name = holder, .score = value}};
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetScore(body, objective, line, protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetScore), body.toVector());
    };
    connection.sendScoreRemoved = [mailbox = joining.mailbox, board, protocol = joining.protocol](
                                      const std::string& holder, const std::optional<std::string>& objective) {
        for (auto& [name, holders] : board->lines) {
            if (objective.has_value() && name != *objective) {
                continue;
            }
            const auto found = holders.find(holder);
            if (found == holders.end()) {
                continue;
            }
            const std::array<std::int64_t, 1> gone{found->second};
            holders.erase(found);
            wire::io::ByteBuffer body;
            wire::bedrock::writeRemoveScores(body, name, gone, protocol);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetScore), body.toVector());
        }
    };
    connection.sendSpawnPosition = [mailbox = joining.mailbox](const world::LoadedWorld& world, const std::int32_t x,
                                                               const std::int32_t y, const std::int32_t z, float) {
        wire::bedrock::BedrockDimension dimension = wire::bedrock::BedrockDimension::Overworld;
        switch (world.declaration().dimension) {
        case config::WorldDimension::Nether:
            dimension = wire::bedrock::BedrockDimension::Nether;
            break;
        case config::WorldDimension::End:
            dimension = wire::bedrock::BedrockDimension::End;
            break;
        case config::WorldDimension::Overworld:
            break;
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetSpawnPosition(body, dimension, x, y, z);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetSpawnPosition), body.toVector());
    };
    // parity: that edition names a mixer and this one cannot, so stopping one mixer stops the lot.
    // A stop that names a sound is the only one it can be exact about.
    connection.stopSound = [mailbox = joining.mailbox](const std::optional<std::string>& sound,
                                                       const std::optional<wire::protocol::play::SoundSource>&) {
        wire::io::ByteBuffer body;
        const std::string_view named = sound.has_value() ? wire::bedrock::bedrockSoundName(*sound) : std::string_view{};
        wire::bedrock::writeStopSound(body, named, named.empty());
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::StopSound), body.toVector());
    };
    // parity: the two that edition drives itself go by their bare name; everything else keeps the
    // whole one, which is what `ClientboundCooldownPacket` carries.
    connection.startItemCooldown = [mailbox = joining.mailbox](const wire::common::Identifier& item,
                                                               const std::int32_t durationTicks) {
        const bool bare = item.path() == "goat_horn" || item.path() == "shield";
        wire::io::ByteBuffer body;
        wire::bedrock::writePlayerStartItemCooldown(body, bare ? item.path() : item.toString(), durationTicks);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlayerStartItemCooldown),
                      body.toVector());
    };
    // parity: this edition has no steps of cracking. It is given a pace and draws the animation
    // itself, which is why what goes out is how much of the block comes off in a turn rather than
    // how far along the break is.
    connection.sendBlockDestruction = [mailbox = joining.mailbox](std::int32_t, const std::int32_t x,
                                                                  const std::int32_t y, const std::int32_t z,
                                                                  const std::int32_t stage, const float perTick) {
        // parity: the other door's tenth step means the cracks come off again.
        constexpr std::int32_t kNoCracks = 10;
        const bool stopping = stage >= kNoCracks;
        if (!stopping && (perTick <= 0.0F || perTick >= 1.0F)) {
            return;
        }
        // How many whole turns it takes, and the range shared out over them: a block that needs
        // three and a half of them needs four.
        const auto ticks = stopping ? 1 : static_cast<std::int32_t>(std::ceil(1.0F / perTick));
        wire::io::ByteBuffer body;
        wire::bedrock::writeLevelEvent(body,
                                       stopping     ? wire::bedrock::LevelEventType::BlockStopBreak
                                       : stage == 0 ? wire::bedrock::LevelEventType::BlockStartBreak
                                                    : wire::bedrock::LevelEventType::BlockUpdateBreak,
                                       static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
                                       stopping ? 0 : wire::bedrock::kBreakProgressScale / ticks);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelEvent), body.toVector());
    };
    connection.sendTime = [mailbox = joining.mailbox](const world::LoadedWorld& world) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetTime(body, static_cast<std::int32_t>(world.clock().totalTicks()));
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetTime), body.toVector());
    };
    connection.sendActionBar = [mailbox = joining.mailbox, lang = context_.lang](const Component& message) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetTitle(body, wire::bedrock::TitleType::ActionBar,
                                     wire::text::toLegacyText(wire::text::resolveComponent(message, lang)), 0,
                                     wire::bedrock::kActionBarTicks, 0);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetTitle), body.toVector());
    };
    // parity: the other door sends a packet per piece and this edition one kind with a type, so
    // the two lists line up one for one. Its action bar lasts as long as it is told rather than a
    // fixed while, which is why the one with no times of its own carries vanilla's.
    connection.sendTitlePiece = [mailbox = joining.mailbox, lang = context_.lang](
                                    const player::TitlePiece piece, const Component& text, const std::int32_t fadeIn,
                                    const std::int32_t stay, const std::int32_t fadeOut) {
        wire::bedrock::TitleType type = wire::bedrock::TitleType::Title;
        switch (piece) {
        case player::TitlePiece::Title:
            break;
        case player::TitlePiece::Subtitle:
            type = wire::bedrock::TitleType::Subtitle;
            break;
        case player::TitlePiece::ActionBar:
            type = wire::bedrock::TitleType::ActionBar;
            break;
        case player::TitlePiece::Times:
            type = wire::bedrock::TitleType::Times;
            break;
        case player::TitlePiece::Clear:
            type = wire::bedrock::TitleType::Clear;
            break;
        case player::TitlePiece::Reset:
            type = wire::bedrock::TitleType::Reset;
            break;
        }
        const std::int32_t hold = piece == player::TitlePiece::ActionBar ? wire::bedrock::kActionBarTicks : stay;
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetTitle(body, type, wire::text::toLegacyText(wire::text::resolveComponent(text, lang)),
                                     fadeIn, hold, fadeOut);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetTitle), body.toVector());
    };
    connection.sendTitle = [mailbox = joining.mailbox, lang = context_.lang](
                               const Component& title, const Component& subtitle, const std::int32_t fadeIn,
                               const std::int32_t stay, const std::int32_t fadeOut) {
        const auto send = [&mailbox](const wire::bedrock::TitleType type, const std::string& text,
                                     const std::int32_t in, const std::int32_t hold, const std::int32_t out) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeSetTitle(body, type, text, in, hold, out);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetTitle), body.toVector());
        };
        // The timings first, as on the other edition: they are read for the title that follows.
        send(wire::bedrock::TitleType::Times, "", fadeIn, stay, fadeOut);
        const std::string under = wire::text::toLegacyText(wire::text::resolveComponent(subtitle, lang));
        if (!under.empty()) {
            send(wire::bedrock::TitleType::Subtitle, under, fadeIn, stay, fadeOut);
        }
        send(wire::bedrock::TitleType::Title, wire::text::toLegacyText(wire::text::resolveComponent(title, lang)),
             fadeIn, stay, fadeOut);
    };

    connection.teleportTo = [this](const double x, const double y, const double z, const float yaw, const float pitch) {
        if (session_ != nullptr) {
            session_->x = x;
            session_->y = y;
            session_->z = z;
            session_->yaw = yaw;
            session_->pitch = pitch;
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeTeleport(body, kRuntimeEntityId, static_cast<float>(x),
                                     static_cast<float>(y) + wire::bedrock::kPlayerEyeHeight, static_cast<float>(z),
                                     yaw, pitch, 0, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::MovePlayer), body.toVector());
    };
    connection.setGamemode = [this](const wire::protocol::play::Gamemode mode) {
        if (session_ != nullptr) {
            session_->gamemode = mode;
        }
        // What the hands may do changes with the mode, and this edition is told in two packets:
        // without them a client keeps whatever it was granted when it arrived.
        sendAbilities();
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetPlayerGameType(body, static_cast<std::int32_t>(mode));
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetPlayerGameType), body.toVector());
    };

    // Rain and thunder are two events rather than a state, and the strength rides along as the
    // whole range of an unsigned short. Thunder is only sent while it is raining: a client told to
    // thunder in clear weather starts raining by itself.
    connection.sendWeather = [mailbox = joining.mailbox](const world::WorldWeather& weather) {
        const auto send = [&mailbox](const wire::bedrock::LevelEventType type, const float strength) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeLevelEvent(
                body, type, 0.0F, 0.0F, 0.0F,
                static_cast<std::int32_t>(strength * static_cast<float>(wire::bedrock::kWeatherStrengthScale)));
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelEvent), body.toVector());
        };
        send(weather.raining ? wire::bedrock::LevelEventType::StartRaining : wire::bedrock::LevelEventType::StopRaining,
             weather.rainLevel());
        if (weather.raining && weather.thundering) {
            send(wire::bedrock::LevelEventType::StartThunderstorm, weather.thunderLevel());
        } else {
            send(wire::bedrock::LevelEventType::StopThunderstorm, 0.0F);
        }
    };

    // Bedrock draws the death screen itself once health reaches zero; this is only the sentence on
    // it. The way back is the client pressing the button, which arrives as a respawn of its own.
    connection.sendCombatDeath = [mailbox = joining.mailbox, lang = context_.lang](const Component& message) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeDeathInfo(body, wire::text::toLegacyText(wire::text::resolveComponent(message, lang)));
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::DeathInfo), body.toVector());
    };
    connection.killPlayer = [this](const std::string_view deathMessageId) {
        if (session_ != nullptr && context_.killed) {
            context_.killed(*session_, deathMessageId);
        }
    };

    connection.hurt = [this](const float amount, const std::string_view deathMessageId,
                             const wire::common::Identifier& damageType,
                             const std::optional<entity::DamageSource>& source) {
        if (session_ != nullptr && context_.hurt) {
            context_.hurt(*session_, amount, deathMessageId, damageType, source);
        }
    };
    connection.heal = [this](const float amount) {
        if (session_ != nullptr && context_.heal) {
            context_.heal(*session_, amount);
        }
    };

    // Bedrock paints the flash off an entity event rather than off a damage type, so the type is
    // read only for whether there is one at all. Its own hearts come off the attributes.
    connection.showDamage = [this](const std::int32_t entityId, std::int32_t, std::int32_t) {
        const std::uint64_t runtime =
            session_ != nullptr && entityId == session_->entityId() ? kRuntimeEntityId : runtimeIdFor(entityId);
        wire::io::ByteBuffer body;
        wire::bedrock::writeEntityEvent(body, runtime, wire::bedrock::BedrockEntityEvent::Hurt, 0, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::EntityEvent), body.toVector());
    };

    // What a command's block change asks the whole world to hear. One connection carries it for
    // everybody, which is why it cannot be this door's own mailbox.
    connection.broadcastEffects = [this](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                         const std::int32_t stateId, const std::vector<world::BlockEffect>& effects) {
        if (session_ != nullptr && context_.blockEffects) {
            context_.blockEffects(session_->worldName, x, y, z, stateId, effects);
        }
    };

    // Morning, or a plugin, getting this player up. The world owns the bed and the pose, so this
    // only asks: the answer comes back as the packet above.
    connection.wakeUp = [this] {
        if (session_ != nullptr && context_.wakeUp) {
            context_.wakeUp(*session_);
        }
    };

    // The book again, after a trade has been taken out of it.
    connection.sendTrades = [this] { sendTrades(); };

    // The three rows of an open enchanting table, which that edition draws from its own packet.
    connection.sendEnchantOptions = [this] { sendEnchantOptions(); };

    // Somebody's emote, which only this edition's clients can draw.
    connection.showEmote = [this](const std::int32_t entityId, const std::string& emoteId, const std::int32_t duration,
                                  const std::uint8_t flags) { showEmote(entityId, emoteId, duration, flags); };

    // Somebody lying down or standing up, which this edition draws off a flag and the bed's own
    // square rather than off a pose.
    connection.showSleeping = [this](const std::int32_t entityId,
                                     const std::optional<std::array<std::int32_t, 3>> bed) {
        const bool itself = session_ != nullptr && entityId == session_->entityId();
        const std::uint64_t runtime = itself ? kRuntimeEntityId : runtimeIdFor(entityId);
        // The second word of flags carries the sleeping bit. Only this player's own carries anything
        // else, so everybody else's is that bit and nothing more.
        std::uint64_t moreFlags =
            itself ? wire::bedrock::flagBit2(wire::bedrock::EntityFlag::PushTowardsClosestSpace) : 0ULL;
        if (bed.has_value()) {
            moreFlags |= wire::bedrock::flagBit2(wire::bedrock::EntityFlag::Sleeping);
        }
        const std::array<std::int32_t, 3> where = bed.value_or(std::array<std::int32_t, 3>{0, 0, 0});
        wire::io::ByteBuffer body;
        wire::bedrock::writeSleeping(body, runtime, moreFlags, where[0], where[1], where[2], protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityData), body.toVector());
    };

    // The shove. Bedrock turns the red on the screen itself from where the motion came, so the
    // angle the Java animation carries has nowhere to go here.
    connection.pushBack = [this](const double velocityX, const double velocityY, const double velocityZ, float) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetEntityMotion(body, kRuntimeEntityId, static_cast<float>(velocityX),
                                            static_cast<float>(velocityY), static_cast<float>(velocityZ));
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityMotion), body.toVector());
    };

    // One column again, for a change too big to patch block by block. Bedrock carries the biomes
    // inside the column, so asking for new biomes is asking for the column: the two share a call.
    connection.resendChunk = [this](const std::int32_t chunkX, const std::int32_t chunkZ) {
        sendColumn(chunkX, chunkZ);
    };
    connection.sendBiomes = [this](const std::vector<std::pair<std::int32_t, std::int32_t>>& columns) {
        for (const auto& [chunkX, chunkZ] : columns) {
            sendColumn(chunkX, chunkZ);
        }
    };

    // The bubbles and the flames. Bedrock draws both off entity metadata rather than off a packet
    // of their own, and the flags have to go out whole every time: a field left out is not a field
    // left alone, it is a field the client keeps at whatever it last heard.
    connection.syncSubmersion = [this] {
        if (session_ == nullptr) {
            return;
        }
        std::uint64_t flags = wire::bedrock::flagBit(wire::bedrock::EntityFlag::HasCollision) |
                              wire::bedrock::flagBit(wire::bedrock::EntityFlag::HasGravity) |
                              wire::bedrock::flagBit(wire::bedrock::EntityFlag::Breathing) |
                              wire::bedrock::flagBit(wire::bedrock::EntityFlag::CanShowName) |
                              wire::bedrock::flagBit(wire::bedrock::EntityFlag::CanClimb);
        // Pushed out of a block over a moment rather than thrown out of it at once, which is what a
        // player who spawns inside one should feel. It lives in the second field, past the 64th.
        const std::uint64_t moreFlags = wire::bedrock::flagBit2(wire::bedrock::EntityFlag::PushTowardsClosestSpace);
        if (session_->fireTicks > 0) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::OnFire);
        }
        if (session_->sneaking) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Sneaking);
        }
        if (session_->sprinting) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Sprinting);
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeSelfMetadata(body, kRuntimeEntityId, static_cast<std::int16_t>(session_->airSupply),
                                         static_cast<std::int16_t>(world::fluids::kMaxAir), flags, moreFlags,
                                         protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityData), body.toVector());
    };

    // parity: Geyser's own wall, which is the only way to show a border to a client that has none:
    // a patch of refusal particles at the face the player is near, six wide and five tall.
    connection.showBorder = [this](const double minX, const double maxX, const double minZ, const double maxZ,
                                   const std::int32_t warningBlocks, const bool withWall) {
        if (session_ == nullptr) {
            return;
        }
        showBorderColliders(minX, maxX, minZ, maxZ, warningBlocks);
        if (!withWall) {
            return;
        }
        /// How close to a face counts as near it, on top of the border's own warning distance.
        constexpr double kCloseToBorder = 5.0;
        constexpr std::int32_t kHalfWidth = 3;
        constexpr std::int32_t kHeight = 5;

        const double warning = static_cast<double>(warningBlocks) + kCloseToBorder;
        const auto puff = [this](const double x, const double y, const double z) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeLevelEvent(body, wire::bedrock::LevelEventType::ParticleDenyBlock,
                                           static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelEvent), body.toVector());
        };
        const auto wall = [&puff, minX, maxX, minZ, maxZ](const double x, const double y, const double z,
                                                          const bool alongZ) {
            const auto base = static_cast<std::int32_t>(y) - 1;
            for (std::int32_t up = base; up < base + kHeight; ++up) {
                const auto middle = static_cast<std::int32_t>(alongZ ? z : x);
                for (std::int32_t at = middle - kHalfWidth; at < middle + kHalfWidth; ++at) {
                    const double other = static_cast<double>(at);
                    if (alongZ && (other < minZ || other > maxZ)) {
                        continue;
                    }
                    if (!alongZ && (other < minX || other > maxX)) {
                        continue;
                    }
                    puff(alongZ ? x : other, static_cast<double>(up), alongZ ? other : z);
                }
            }
        };
        if (session_->x > maxX - warning) {
            wall(maxX + 0.5, session_->y, session_->z, true);
        }
        if (session_->x < minX + warning) {
            wall(minX - 0.5, session_->y, session_->z, true);
        }
        if (session_->z > maxZ - warning) {
            wall(session_->x, session_->y, maxZ + 0.5, false);
        }
        if (session_->z < minZ + warning) {
            wall(session_->x, session_->y, minZ - 0.5, false);
        }
    };

    connection.setCamera = [this](const std::int32_t entityId) { setCamera(entityId); };

    // parity: Geyser's own dialog cache, which turns a dialog into the only screen this edition
    // has for one: a title, the text, and a button per action.
    connection.showDialog = [this](std::int32_t, const world::dialogs::Dialog& dialog) { showDialog(dialog); };

    // parity: Geyser's own waypoint cache. The other edition draws its bar from a list of its own
    // rather than from one packet per waypoint, and the dot's icon is picked here from how far
    // away the viewer is instead of by the client from a style it was given.
    connection.sendWaypoint = [this](const wire::common::Uuid& waypoint, const std::int32_t entityId,
                                     const std::string&, const std::optional<std::int32_t>& color, const double x,
                                     const double y, const double z, const bool tracked) {
        if (session_ == nullptr) {
            return;
        }
        const std::uint64_t runtime = entityId == session_->entityId() ? kRuntimeEntityId : runtimeIdFor(entityId);
        wire::io::ByteBuffer body;
        // Below 944 there is no bar packet at all: the bar is built out of the player list, so
        // only somebody already listed can be pointed at, and it is hidden while nothing is on it.
        if (protocol_ < wire::bedrock::kLocatorBarProtocol) {
            if (tracked) {
                waypoints_.insert(waypoint);
            } else {
                waypoints_.erase(waypoint);
            }
            wire::bedrock::writeGameRuleFlag(body, "locatorBar", !waypoints_.empty(), protocol_);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::GameRulesChanged), body.toVector());
            wire::io::ByteBuffer where;
            wire::bedrock::writePlayerLocation(
                where, static_cast<std::int64_t>(runtime),
                tracked ? std::optional{std::array{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}}
                        : std::nullopt,
                protocol_);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlayerLocation), where.toVector());
            return;
        }
        const double awayX = x - session_->x;
        const double awayY = y - session_->y;
        const double awayZ = z - session_->z;
        const wire::bedrock::BedrockWaypoint entry{
            .groupHigh = waypoint.high(),
            .groupLow = waypoint.low(),
            .x = static_cast<float>(x),
            .y = static_cast<float>(y),
            .z = static_cast<float>(z),
            .dimension = dimension_,
            .colour = color.value_or(waypointColourOf(waypoint)),
            .distance = static_cast<float>(std::sqrt((awayX * awayX) + (awayY * awayY) + (awayZ * awayZ))),
            .entityId = static_cast<std::int64_t>(runtime)};
        // A waypoint the client already has is moved rather than added again, which is what keeps
        // it from flickering while somebody walks.
        const bool known = tracked && !waypoints_.insert(waypoint).second;
        if (!tracked) {
            waypoints_.erase(waypoint);
        }
        const auto action = !tracked ? wire::bedrock::BedrockWaypointAction::Remove
                            : known  ? wire::bedrock::BedrockWaypointAction::Update
                                     : wire::bedrock::BedrockWaypointAction::Add;
        wire::bedrock::writeLocatorBar(body, entry, action, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LocatorBar), body.toVector());
    };

    // parity: the other edition's client builds the name over a head out of the team packet. This
    // one has no teams at all, so it is told the line already built.
    connection.sendNameTag = [this](const std::int32_t entityId, const wire::text::Component& nameTag) {
        const std::uint64_t runtime =
            session_ != nullptr && entityId == session_->entityId() ? kRuntimeEntityId : runtimeIdFor(entityId);
        wire::io::ByteBuffer body;
        wire::bedrock::writeNameTag(body, runtime, wire::text::toLegacyText(nameTag), true, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityData), body.toVector());
    };

    // Everything they carry. Bedrock numbers its own inventory the other way round from Java: the
    // hotbar is the first nine slots there and the last nine here.
    connection.sendInventoryAll = [this] {
        if (session_ == nullptr) {
            return;
        }
        const player::PlayerInventory& inventory = session_->inventory;
        std::array<wire::bedrock::BedrockItemStack, player::PlayerInventory::kMainSlots> carried{};
        for (std::int32_t slot = 0; slot < player::PlayerInventory::kMainSlots; ++slot) {
            const std::int32_t from = slot < player::PlayerInventory::kHotbarSize
                                          ? player::PlayerInventory::kHotbarStart + slot
                                          : slot - player::PlayerInventory::kHotbarSize;
            carried[static_cast<std::size_t>(slot)] = stackFor(inventory.at(from));
        }
        sendWindow(0, carried, wire::bedrock::BedrockContainerSlot::HotbarAndInventory);

        std::array<wire::bedrock::BedrockItemStack, player::PlayerInventory::kArmourSlots> worn{};
        for (std::int32_t slot = 0; slot < player::PlayerInventory::kArmourSlots; ++slot) {
            worn[static_cast<std::size_t>(slot)] = stackFor(inventory.at(player::PlayerInventory::kArmourStart + slot));
        }
        sendWindow(kArmourContainer, worn, wire::bedrock::BedrockContainerSlot::Armour);

        const std::array<wire::bedrock::BedrockItemStack, 1> other{
            stackFor(inventory.at(player::PlayerInventory::kOffhandSlot))};
        sendWindow(kOffhandContainer, other, wire::bedrock::BedrockContainerSlot::Offhand);
    };

    // The hotbar is the first nine slots there, so the number is the same one either edition sends
    // back when the player moves their own hand.
    connection.setHeldSlot = [mailbox = joining.mailbox](const std::int32_t slot) {
        wire::io::ByteBuffer body;
        wire::bedrock::writePlayerHotbar(body, slot);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlayerHotbar), body.toVector());
    };

    // Another world is another dimension as far as a Bedrock client is concerned: the sky, the fog
    // and the chunk cache all hang off it. What the world means by "somewhere else" stays with the
    // caller; this only draws the arrival.
    connection.switchWorld = [this](world::LoadedWorld& target, const double x, const double y,
                                    const double z) -> bool {
        if (session_ == nullptr || !context_.changeWorld) {
            return false;
        }
        const std::optional<Context::Arrival> arrival = context_.changeWorld(*session_, target, x, y, z);
        if (!arrival.has_value()) {
            return false;
        }
        session_->worldName = arrival->worldName;
        dimension_ = arrival->dimension;
        session_->x = x;
        session_->y = y;
        session_->z = z;
        // Everybody drawn so far was in the world just left, so the next turn draws the new one
        // from nothing rather than moving people who are no longer there.
        others_.clear();

        wire::io::ByteBuffer body;
        wire::bedrock::writeChangeDimension(body, arrival->dimension, static_cast<float>(x),
                                            static_cast<float>(y) + wire::bedrock::kPlayerEyeHeight,
                                            static_cast<float>(z), false);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ChangeDimension), body.toVector());
        return true;
    };

    // A sound, under the name its own edition knows it by. One that Bedrock has no name for is not
    // played rather than played wrong: a wrong sound is worse than a silent one.
    connection.playSound = [mailbox = joining.mailbox,
                            protocol = joining.protocol](const std::string_view sound, const double x, const double y,
                                                         const double z, const float volume, const float pitch) {
        const std::string_view named = wire::bedrock::bedrockSoundName(sound);
        if (!named.empty()) {
            wire::io::ByteBuffer body;
            wire::bedrock::writePlaySound(body, named, x, y, z, volume, pitch, protocol);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlaySound), body.toVector());
            return;
        }
        // A sound with no name of its own is a numbered event over there, which is how the hits of
        // a fight reach that client at all: none of the three has a name.
        const std::int32_t event = wire::bedrock::bedrockSoundEvent(sound, protocol);
        if (event < 0) {
            return;
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeLevelSoundEvent(body, event, static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(z), protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelSoundEvent), body.toVector());
    };

    // A particle, under the name its own edition knows it by. One that Bedrock only draws from a
    // numbered event has no name to send, so it is left undrawn rather than drawn as something else.
    connection.showParticle = [this](const std::string_view particle, const double x, const double y, const double z) {
        const std::string_view named = wire::bedrock::bedrockParticleName(particle);
        if (named.empty()) {
            return;
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeSpawnParticle(body, dimension_, named, static_cast<float>(x), static_cast<float>(y),
                                          static_cast<float>(z));
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SpawnParticleEffect), body.toVector());
    };

    // What a block asked to be done with it. The two numbers are the same on both editions, so
    // the only thing this does is write them where that one reads them.
    connection.sendBlockEvent = [mailbox = joining.mailbox](const std::int32_t x, const std::int32_t y,
                                                            const std::int32_t z, const std::int32_t,
                                                            const std::int32_t b0, const std::int32_t b1) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeBlockEvent(body, x, y, z, b0, b1);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BlockEvent), body.toVector());
    };

    // One of the numbered one-off effects. That edition splits what Java's single number carries
    // into an effect and a sound, and has no answer at all for most of them: one it cannot draw is
    // left undrawn rather than drawn as something else.
    const auto records = std::make_shared<BedrockRecords>(
        joining.protocol, [mailbox = joining.mailbox](wire::bedrock::BedrockPacket id, std::vector<std::uint8_t> body) {
            mailbox->post(static_cast<std::uint16_t>(id), std::move(body));
        });
    connection.sendLevelEvent = [mailbox = joining.mailbox, protocol = joining.protocol, records, lang = context_.lang](
                                    const std::int32_t type, const std::int32_t x, const std::int32_t y,
                                    const std::int32_t z, const std::int32_t data) {
        // parity: Geyser's `SOUND_PLAY_JUKEBOX_SONG` and `SOUND_STOP_JUKEBOX_SONG`, with the popup
        // Java draws itself sent as a jukebox line.
        if (type == world::blockEffect::Effect::kStopJukeboxSong) {
            records->stopped(x, y, z);
            return;
        }
        if (type == world::blockEffect::Effect::kPlayJukeboxSong) {
            const std::vector<registry::vanillaRegistryData::Entry>& songs =
                registry::vanillaRegistryData::jukeboxSongs();
            if (data < 0 || static_cast<std::size_t>(data) >= songs.size()) {
                return;
            }
            const auto& [song, definition] = songs[static_cast<std::size_t>(data)];
            records->started(x, y, z, definition.getString("sound_event"));
            const Component playing = Component::translatable(
                "record.nowPlaying", {Component::translatable("jukebox_song.minecraft." + song)});
            wire::io::ByteBuffer popup;
            wire::bedrock::writeText(popup, wire::bedrock::TextType::JukeboxPopup, "",
                                     wire::text::toLegacyText(wire::text::resolveComponent(playing, lang)));
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Text), popup.toVector());
            return;
        }
        const wire::bedrock::BedrockLevelEffect effect = wire::bedrock::bedrockLevelEffect(type, data);
        if (effect.event != 0) {
            const std::int32_t carried = effect.translatesBlock
                                             ? static_cast<std::int32_t>(wire::bedrock::bedrockBlockId(data, protocol))
                                             : data;
            wire::io::ByteBuffer body;
            wire::bedrock::writeLevelEvent(body, effect.event, static_cast<float>(x) + effect.x,
                                           static_cast<float>(y) + effect.y, static_cast<float>(z) + effect.z,
                                           effect.keepsData ? carried : 0);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelEvent), body.toVector());
        }
        if (effect.sound.empty()) {
            return;
        }
        const std::string_view named = wire::bedrock::bedrockSoundName(effect.sound);
        if (named.empty()) {
            return;
        }
        wire::io::ByteBuffer sound;
        wire::bedrock::writePlaySound(sound, named, static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                                      static_cast<double>(z) + 0.5, 1.0F, 1.0F, protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlaySound), sound.toVector());
    };

    // The numbers of an open furnace, once per turn of the world. It carries the windows rather
    // than the player because it runs where the world runs, and by then this player may be gone.
    connection.syncOpenWindow = [mailbox = joining.mailbox, protocol = joining.protocol,
                                 windows = windows_](world::LoadedWorld& world) {
        // A window with none of the block's own slots has nothing the world can change under it:
        // a table's grid is the player's, and so is an ender chest. Both only move when they do.
        if (!windows->open.has_value() || windows->open->size == 0 || windows->open->playerOwned) {
            return;
        }
        // What it holds, for the turns nobody touched it: a furnace fills its own result slot, and
        // somebody else may be in the same chest. Not every turn: see `sinceRead`.
        if (++windows->sinceRead >= kWindowReadTurns) {
            windows->sinceRead = 0;
            std::vector<std::optional<storage::StoredItemStack>> slots = readWindow(world, *windows->open, nullptr);
            if (slots != windows->saidSlots) {
                windows->saidSlots = std::move(slots);
                drawWindowSlots(mailbox, protocol, *windows->open, windows->saidSlots);
            }
        }
        const std::optional<FurnaceNumbers> numbers = furnaceNumbers(world, *windows->open);
        if (!numbers.has_value() || (windows->saidNumbers.has_value() && *windows->saidNumbers == *numbers)) {
            return;
        }
        windows->saidNumbers = numbers;
        const std::int32_t containerId = windows->open->containerId;
        const auto say = [&mailbox, containerId](const wire::bedrock::BedrockContainerData property,
                                                 const std::int32_t value) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeContainerSetData(body, containerId, property, value);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ContainerSetData), body.toVector());
        };
        say(wire::bedrock::BedrockContainerData::FurnaceCookTime, numbers->cookTime);
        say(wire::bedrock::BedrockContainerData::FurnaceLitTime, numbers->litTime);
        say(wire::bedrock::BedrockContainerData::FurnaceLitDuration, numbers->litDuration);
    };

    connection.showBossBar = [this](const wire::common::Uuid& id, const Component& title, const float progress,
                                    const bool show) {
        // One entity id per bar, taken from the uuid: over here a bar belongs to something in the
        // world rather than to an id of its own, and two bars may not share one.
        const auto bossId = static_cast<std::int64_t>((id.high() ^ id.low()) | 1U);
        wire::io::ByteBuffer body;
        const bool drawn = shownBars_.contains(bossId);
        if (!show) {
            if (!drawn) {
                return;
            }
            wire::bedrock::writeBossEvent(body, bossId, wire::bedrock::BedrockBossAction::Remove, "", 0.0F, protocol_);
            shownBars_.erase(bossId);
        } else {
            const std::string text = wire::text::toLegacyText(wire::text::resolveComponent(title, context_.lang));
            wire::bedrock::writeBossEvent(body, bossId,
                                          drawn ? wire::bedrock::BedrockBossAction::UpdatePercentage
                                                : wire::bedrock::BedrockBossAction::Create,
                                          text, progress, protocol_);
            shownBars_.insert(bossId);
        }
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BossEvent), body.toVector());
    };

    connection.setDifficulty = [mailbox = joining.mailbox](const std::int32_t difficulty, bool) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetDifficulty(body, difficulty);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetDifficulty), body.toVector());
    };

    connection.sendPassengers = [mailbox = joining.mailbox,
                                 seen = std::make_shared<std::map<std::int32_t, std::vector<std::int32_t>>>()](
                                    const std::int32_t vehicleId, const std::vector<std::int32_t>& riders) {
        const auto post = [&mailbox](const std::int64_t vehicle, const std::int64_t rider,
                                     const wire::bedrock::BedrockRide how) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeSetEntityLink(body, vehicle, rider, how, true);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityLink), body.toVector());
        };
        // Whoever was on it and is not any more has to be taken off by name: this edition has no
        // packet that says "these are all of them now".
        std::vector<std::int32_t>& before = (*seen)[vehicleId];
        for (const std::int32_t was : before) {
            if (std::ranges::find(riders, was) == riders.end()) {
                post(static_cast<std::int64_t>(runtimeIdFor(vehicleId)), static_cast<std::int64_t>(runtimeIdFor(was)),
                     wire::bedrock::BedrockRide::Remove);
            }
        }
        for (std::size_t at = 0; at < riders.size(); ++at) {
            if (std::ranges::find(before, riders[at]) != before.end()) {
                continue;
            }
            // The first one steers; anybody after that is along for the trip.
            post(static_cast<std::int64_t>(runtimeIdFor(vehicleId)),
                 static_cast<std::int64_t>(runtimeIdFor(riders[at])),
                 at == 0 ? wire::bedrock::BedrockRide::Rider : wire::bedrock::BedrockRide::Passenger);
        }
        before = riders;
    };

    connection.openSignEditor = [mailbox = joining.mailbox](const std::int32_t x, const std::int32_t y,
                                                            const std::int32_t z) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeOpenSign(body, x, y, z, true);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::OpenSign), body.toVector());
    };

    connection.sendSignText = [mailbox = joining.mailbox](const std::int32_t x, const std::int32_t y,
                                                          const std::int32_t z, const std::string& front,
                                                          const std::string& back, const bool waxed) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeSignData(body, x, y, z, front, back, waxed);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BlockEntityData), body.toVector());
    };

    connection.transferTo = [mailbox = joining.mailbox, protocol = joining.protocol](const std::string& host,
                                                                                     const std::int32_t port) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeTransfer(body, host, static_cast<std::uint16_t>(port), protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Transfer), body.toVector(), true);
    };

    connection.sendSwing = [mailbox = joining.mailbox](const std::int32_t entityId, bool) {
        // One arm on Bedrock: there is no packet for the other hand, and drawing the wrong arm is
        // worse than drawing the one everybody expects.
        wire::io::ByteBuffer body;
        wire::bedrock::writeAnimate(body, runtimeIdFor(entityId), wire::bedrock::BedrockAnimation::SwingArm);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Animate), body.toVector());
    };

    connection.showCriticalHit = [mailbox = joining.mailbox](const std::int32_t entityId) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeAnimate(body, runtimeIdFor(entityId), wire::bedrock::BedrockAnimation::CriticalHit);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Animate), body.toVector());
    };

    connection.sendPickupAnimation = [mailbox = joining.mailbox](const std::int32_t itemEntityId,
                                                                 const std::int32_t collectorEntityId) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeTakeItemEntity(body, runtimeIdFor(itemEntityId), runtimeIdFor(collectorEntityId));
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::TakeItemEntity), body.toVector());
    };

    connection.sendExplosion = [mailbox = joining.mailbox, protocol = joining.protocol](const double x, const double y,
                                                                                        const double z) {
        wire::io::ByteBuffer effect;
        wire::bedrock::writeLevelEvent(effect, wire::bedrock::LevelEventType::ParticleExplosion, static_cast<float>(x),
                                       static_cast<float>(y), static_cast<float>(z), 0);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelEvent), effect.toVector());
        wire::io::ByteBuffer sound;
        wire::bedrock::writePlaySound(sound, "random.explode", x, y, z, 4.0F, 1.0F, protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlaySound), sound.toVector());
    };

    // A block that changed under somebody's feet. Only reaches a client that already has the
    // column: one that does not will get it whole when it comes into range.
    connection.sendBlockChange = [mailbox = joining.mailbox,
                                  protocol = joining.protocol](const std::int32_t x, const std::int32_t y,
                                                               const std::int32_t z, const std::int32_t stateId) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeUpdateBlock(body, x, y, z, stateId, protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateBlock), body.toVector());
        // A bed that was just put down is one block over there and sixteen here, so the colour
        // travels behind the block rather than in it.
        if (const auto colour = bedColourOf(stateId); colour) {
            wire::io::ByteBuffer bed;
            wire::bedrock::writeBedData(bed, x, y, z, *colour);
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BlockEntityData), bed.toVector());
        }
    };

    connection.sendCommandTree = [this] {
        if (session_ == nullptr || !context_.commandList) {
            return;
        }
        // TEMPORARY: the one packet the join burst never contained, and the only thing `/op` sends.
        if (const char* const drop = std::getenv("BASALT_BEDROCK_DROP");
            drop != nullptr && std::string_view{drop}.find("commandtree") != std::string_view::npos) {
            context_.logger.warn([] { return "Leaving out of the Bedrock join: commandtree"; });
            return;
        }
        // TEMPORARY: six of the names a server operator is offered carry a hyphen, and Bedrock has
        // no native command that does. The thirteen a plain player gets have none, which is the
        // difference between the tree that worked and the tree that did not.
        const char* const drop = std::getenv("BASALT_BEDROCK_DROP");
        const bool withoutHyphens = drop != nullptr && std::string_view{drop}.find("hyphens") != std::string_view::npos;
        std::vector<wire::bedrock::CommandInfo> commands;
        for (auto& [name, usage] : context_.commandList(*session_)) {
            if (withoutHyphens && name.find('-') != std::string::npos) {
                continue;
            }
            commands.push_back(wire::bedrock::CommandInfo{std::move(name), fitsInACommandTip(usage)});
        }
        if (withoutHyphens) {
            context_.logger.warn([] { return "Leaving out of the Bedrock command tree: hyphenated names"; });
        }
        // TEMPORARY: `BASALT_BEDROCK_COMMANDS=n` offers only the first n, which is how to ask a
        // client which entry it cannot take when the packet is otherwise well formed.
        if (const char* const most = std::getenv("BASALT_BEDROCK_COMMANDS"); most != nullptr) {
            const auto kept = static_cast<std::size_t>(std::atoll(most));
            if (commands.size() > kept) {
                commands.resize(kept);
            }
            context_.logger.warn([&commands] {
                std::string names;
                for (std::size_t at = commands.size() >= 8 ? commands.size() - 8 : 0; at < commands.size(); ++at) {
                    names += (names.empty() ? "" : ", ") + commands[at].name;
                }
                return "Offering only " + std::to_string(commands.size()) +
                       " Bedrock command(s), the last of them: " + names;
            });
        }
        // TEMPORARY: the whole list in order, which is how to name the one a client will not take.
        context_.logger.debug([&commands] {
            std::string named;
            for (std::size_t at = 0; at < commands.size(); ++at) {
                named += (named.empty() ? "" : " ") + std::to_string(at) + ":" + commands[at].name;
            }
            return "Bedrock commands offered: " + named;
        });
        wire::io::ByteBuffer body;
        wire::bedrock::writeAvailableCommands(body, commands);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::AvailableCommands), body.toVector());
    };

    // The three effect callbacks are the same packet with a different word in it, so they share a
    // writer. An effect Bedrock has no number for is not sent: a wrong icon is worse than none.
    connection.addEffect = [this](const wire::common::Identifier& effect, const std::int32_t durationTicks,
                                  const std::int32_t amplifier) {
        sendEffect(effect, wire::bedrock::BedrockEffectEvent::Add, durationTicks, amplifier, false, true);
    };
    connection.sendEffect = [this](const world::ActiveEffect& effect) {
        sendEffect(effect.effect, wire::bedrock::BedrockEffectEvent::Add, effect.remainingTicks, effect.amplifier,
                   effect.ambient, effect.visible);
    };
    connection.sendEffectRemoved = [this](const wire::common::Identifier& effect) {
        sendEffect(effect, wire::bedrock::BedrockEffectEvent::Remove, 0, 0, false, false);
    };

    // Bedrock draws its hearts and haunches from attributes, and is told the range of each one:
    // where Java hardcodes the bounds in the client, here the server states them.
    connection.syncHealth = [this] {
        if (session_ == nullptr) {
            return;
        }
        const std::array<wire::bedrock::AttributeValue, 5> values{{
            {"minecraft:health", 0.0F, session_->maxHealth(), session_->health(), session_->maxHealth()},
            {"minecraft:player.hunger", 0.0F, 20.0F, static_cast<float>(session_->food.foodLevel()), 20.0F},
            {"minecraft:player.saturation", 0.0F, 20.0F, session_->food.saturationLevel(), 20.0F},
            {"minecraft:absorption", 0.0F, 1024.0F, session_->absorption, 0.0F},
            // How fast they walk. Bedrock takes this from the attribute and not from the speed in
            // the ability layer, and a client never told it walks at a pace of its own choosing.
            {"minecraft:movement", 0.0F, 1024.0F, kWalkSpeed, kWalkSpeed},
        }};
        wire::io::ByteBuffer body;
        wire::bedrock::writeUpdateAttributes(body, kRuntimeEntityId, values, 0);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateAttributes), body.toVector());
    };
    connection.syncAbsorption = connection.syncHealth;
    connection.syncExperience = [this] {
        if (session_ == nullptr) {
            return;
        }
        const std::array<wire::bedrock::AttributeValue, 2> values{{
            {"minecraft:player.experience", 0.0F, 1.0F, session_->experience.progress(), 0.0F},
            {"minecraft:player.level", 0.0F, 24791.0F, static_cast<float>(session_->experience.level()), 0.0F},
        }};
        wire::io::ByteBuffer body;
        wire::bedrock::writeUpdateAttributes(body, kRuntimeEntityId, values, 0);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateAttributes), body.toVector());
    };

    // A window of items is a list of buttons over here, and the plugin never learns the difference.
    connection.openPluginMenu =
        [this](const std::string& title, std::int32_t, const std::map<std::int32_t, server::plugins::PluginItem>& items,
               std::function<void(std::int32_t)> onClick) { showMenu(title, items, std::move(onClick)); };
    connection.refreshPluginMenu = [this](const std::map<std::int32_t, server::plugins::PluginItem>& items) {
        // Drawn again rather than patched: a form is not a window, and a client redraws whole the
        // one it is given.
        std::function<void(std::int32_t)> onClick;
        std::string title;
        {
            const std::lock_guard<std::mutex> guard{formLock_};
            if (!form_.has_value()) {
                return;
            }
            onClick = form_->onClick;
            title = openTitle_;
        }
        showMenu(title, items, std::move(onClick));
    };
    connection.closeOpenMenu = [this] {
        const std::lock_guard<std::mutex> guard{formLock_};
        form_.reset();
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ClientboundCloseForm), {});
    };

    // What players.db already knows, asked for before a single packet goes out: the client is told
    // its position and its game mode as it arrives, so restoring afterwards would announce one
    // state and then correct it.
    connection.storedSnapshot = [this]() -> std::optional<storage::StoredPlayer> {
        if (session_ == nullptr) {
            return std::nullopt;
        }
        return player::playerPersistence::toStored(*session_, session_->worldName, firstSeenEpochSeconds_,
                                                   playTimeTicks_, nowEpochSeconds());
    };
    return connection;
}

} // namespace basalt::server::runtime::bedrock
