#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/wire/bedrock/BedrockBlocks.hpp>
#include <basalt/wire/bedrock/BedrockEffects.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockParticles.hpp>
#include <basalt/wire/bedrock/BedrockSounds.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <span>
#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

using wire::text::Component;

/// What `start_game` told this client it is. Every client is told the same number: a Bedrock
/// session only ever draws one player of its own.
constexpr std::uint64_t kRuntimeEntityId = 1;

/// The two windows a player has beside the one they carry. Neither number has moved in this window.
constexpr std::uint32_t kOffhandContainer = 119;
constexpr std::uint32_t kArmourContainer = 120;

/// How often the rest of the server is looked at, in the listener's own loop. The world moves once
/// every fifty milliseconds and the loop turns faster than that, so this is what a tick is worth.
constexpr std::chrono::milliseconds kSyncPeriod{50};

/// How far around a player the world is sent when they arrive. The same distance the door already
/// told the client to draw.
constexpr std::int32_t kChunkRadius = 4;

[[nodiscard]] std::int64_t nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/// A position this client has already been told is the same position: a double that came off the
/// wire as a float never lands exactly where it started.
constexpr double kMoveEpsilon = 0.01;
constexpr float kTurnEpsilon = 1.0F;

/// Somebody else's number, as this client knows it. One is the client's own, from `start_game`,
/// and every entity id is one or more, so they are all moved out of the way by exactly that.
[[nodiscard]] std::uint64_t runtimeIdFor(const std::int32_t entityId) {
    return static_cast<std::uint64_t>(entityId) + 1;
}

} // namespace

BedrockPlayer::BedrockPlayer(const BedrockListener::Joining& joining, Context context)
    : context_(std::move(context)), mailbox_(joining.mailbox), protocol_(joining.protocol) {
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
        wire::bedrock::writeDisconnect(body, wire::text::toLegacyText(wire::text::resolveComponent(reason, lang)));
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Disconnect), body.toVector(), true);
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
                              wire::bedrock::flagBit(wire::bedrock::EntityFlag::CanShowName);
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
        wire::bedrock::writeSubmersion(body, kRuntimeEntityId, static_cast<std::int16_t>(session_->airSupply), flags,
                                       protocol_);
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
        if (named.empty()) {
            return;
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writePlaySound(body, named, x, y, z, volume, pitch, protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlaySound), body.toVector());
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

    // A block that changed under somebody's feet. Only reaches a client that already has the
    // column: one that does not will get it whole when it comes into range.
    connection.sendBlockChange = [mailbox = joining.mailbox,
                                  protocol = joining.protocol](const std::int32_t x, const std::int32_t y,
                                                               const std::int32_t z, const std::int32_t stateId) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeUpdateBlock(body, x, y, z, stateId, protocol);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateBlock), body.toVector());
    };

    connection.sendCommandTree = [this] {
        if (session_ == nullptr || !context_.commandList) {
            return;
        }
        std::vector<wire::bedrock::CommandInfo> commands;
        for (auto& [name, usage] : context_.commandList(*session_)) {
            commands.push_back(wire::bedrock::CommandInfo{std::move(name), std::move(usage)});
        }
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
        const std::array<wire::bedrock::AttributeValue, 4> values{{
            {"minecraft:health", 0.0F, session_->maxHealth(), session_->health(), session_->maxHealth()},
            {"minecraft:player.hunger", 0.0F, 20.0F, static_cast<float>(session_->food.foodLevel()), 20.0F},
            {"minecraft:player.saturation", 0.0F, 20.0F, session_->food.saturationLevel(), 20.0F},
            {"minecraft:absorption", 0.0F, 1024.0F, session_->absorption, 0.0F},
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

    const std::int32_t entityId = context_.registry != nullptr ? context_.registry->allocateEntityId() : 1;
    session_ = std::make_unique<player::PlayerSession>(joining.uuid, joining.name, entityId, std::move(connection));
    session_->x = context_.spawnX;
    session_->y = context_.spawnY;
    session_->z = context_.spawnZ;
    session_->worldName = context_.worldName;

    if (context_.stored) {
        if (const std::optional<storage::StoredPlayer> stored = context_.stored(joining.uuid); stored.has_value()) {
            player::playerPersistence::restore(*session_, *stored);
            firstSeenEpochSeconds_ = stored->firstSeenEpochSeconds;
            playTimeTicks_ = stored->playTimeTicks;
            // Only the *position* is meaningless when the stored world is gone. Health, hunger,
            // experience and game mode are the player's, not the world's.
            if (stored->world != context_.worldName) {
                session_->x = context_.spawnX;
                session_->y = context_.spawnY;
                session_->z = context_.spawnZ;
            } else {
                session_->worldName = stored->world;
            }
            context_.logger.info([this] { return "Loaded " + session_->name() + " from players.db."; });
        }
    }

    if (context_.registry != nullptr) {
        context_.registry->add(session_.get());
    }
    // What the HUD draws, before anything can change it: a client that is told nothing draws a
    // player with no hearts at all.
    session_->connection().syncHealth();
    session_->connection().syncExperience();
    session_->connection().sendInventoryAll();
    // The world itself, once the player exists. The columns are built where the world runs and
    // arrive here already written, so the door's thread never reads a chunk.
    //
    // And the client is only told it has spawned once they are all on their way: telling it first
    // drops somebody into a world that is not there yet, which is what the bots caught.
    const auto spawned = [mailbox = mailbox_] {
        wire::io::ByteBuffer status;
        wire::bedrock::writePlayStatus(status, wire::bedrock::PlayStatus::PlayerSpawn);
        mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlayStatus), status.toVector());
    };
    if (!sendColumnsAround(spawned)) {
        // No world to send means no reason to wait for it.
        spawned();
    }

    session_->connection().sendWeather(context_.weather);
    session_->connection().sendCommandTree();
    wire::io::ByteBuffer time;
    wire::bedrock::writeSetTime(time, context_.worldTime);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetTime), time.toVector());
    announce(Component::translatable("multiplayer.player.joined", {Component::text(session_->name())})
                 .color(wire::text::kYellow));
    // Last, once the player exists everywhere: a plugin that opens a menu on the join event has to
    // find a player who is already in the registry and can already be sent something.
    if (context_.joined) {
        context_.joined(*session_);
    }
}

BedrockPlayer::~BedrockPlayer() {
    if (session_ == nullptr) {
        return;
    }
    // Written before they leave the registry, so what is saved is a player who still exists.
    if (context_.save && session_->connection().storedSnapshot) {
        if (const std::optional<storage::StoredPlayer> row = session_->connection().storedSnapshot(); row.has_value()) {
            context_.save(*row);
        }
    }
    if (context_.quit) {
        context_.quit(*session_);
    }
    announce(Component::translatable("multiplayer.player.left", {Component::text(session_->name())})
                 .color(wire::text::kYellow));
    if (context_.registry != nullptr) {
        context_.registry->remove(session_->uuid());
        // Handed over rather than deleted here: this runs on the listener's thread, and the tick
        // loop may be halfway through a list of players that still has this one in it.
        context_.registry->retire(std::move(session_));
    }
}

void BedrockPlayer::chat(const std::string_view message) {
    if (session_ == nullptr || message.empty()) {
        return;
    }
    if (!context_.chatFilter) {
        say(std::string{message});
        return;
    }
    // A plugin may rewrite the line or stop it, and it only gets to do that where plugins run, so
    // the line is handed over and comes back with the answer.
    context_.chatFilter(*session_, std::string{message}, [this](const bool allowed, std::string spoken) {
        if (allowed) {
            say(std::move(spoken));
        }
    });
}

wire::bedrock::BedrockItemStack BedrockPlayer::stackFor(const std::optional<storage::StoredItemStack>& stack) const {
    if (!stack.has_value() || stack->count <= 0) {
        return {};
    }
    const wire::bedrock::ItemName* named = wire::bedrock::bedrockItem(stack->id);
    if (named == nullptr) {
        return {};
    }
    // An item that places a block carries which one: the client draws the block from that and not
    // from the item's own name.
    std::int32_t block = 0;
    if (const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(stack->id);
        id.has_value()) {
        if (const std::optional<std::int32_t> state = server::registry::blocks::defaultStateIdOf(*id);
            state.has_value()) {
            block = static_cast<std::int32_t>(wire::bedrock::bedrockBlockId(*state, protocol_));
        }
    }
    return {.runtimeId = named->palettePlace + 1, .count = stack->count, .data = named->data, .blockRuntimeId = block};
}

void BedrockPlayer::sendWindow(const std::uint32_t containerId,
                               const std::span<const wire::bedrock::BedrockItemStack> items,
                               const wire::bedrock::BedrockContainerSlot slot) {
    wire::io::ByteBuffer body;
    wire::bedrock::writeInventoryContent(body, containerId, items, slot, protocol_);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::InventoryContent), body.toVector());
}

void BedrockPlayer::sendEffect(const wire::common::Identifier& effect, const wire::bedrock::BedrockEffectEvent event,
                               const std::int32_t durationTicks, const std::int32_t amplifier, const bool ambient,
                               const bool particles) {
    const std::optional<std::int32_t> java = world::mobEffects::protocolIdOf(effect);
    if (!java.has_value()) {
        return;
    }
    const std::int32_t id = wire::bedrock::bedrockEffectId(*java);
    if (id == 0) {
        return;
    }
    wire::io::ByteBuffer body;
    // parity: Java counts an infinite effect as -1 and Bedrock has no such thing, so it is sent as
    // the longest one that edition can hold instead of as a negative that reads as already over.
    const std::int32_t ticks = durationTicks < 0 ? std::numeric_limits<std::int32_t>::max() : durationTicks;
    wire::bedrock::writeMobEffect(body, kRuntimeEntityId, event, id, amplifier, particles, ticks, ambient);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::MobEffect), body.toVector());
}

void BedrockPlayer::blockAction(const std::int32_t action, const std::int32_t x, const std::int32_t y,
                                const std::int32_t z, std::int32_t) {
    if (session_ == nullptr || !context_.breakBlock) {
        return;
    }
    // A client that predicts the break has already taken the block away on its own screen, which is
    // the one that counts here: everything else is told by the world. The other four say a dig is
    // starting, going on or given up, and this server draws no cracks yet.
    if (action != static_cast<std::int32_t>(wire::bedrock::BedrockBlockActionType::PredictDestroy) &&
        action != static_cast<std::int32_t>(wire::bedrock::BedrockBlockActionType::StopBreak)) {
        return;
    }
    context_.breakBlock(*session_, x, y, z);
}

void BedrockPlayer::command(const std::string_view line) {
    if (session_ == nullptr || line.empty() || !context_.runCommand) {
        return;
    }
    context_.runCommand(*session_, std::string{line});
}

void BedrockPlayer::say(std::string message) {
    if (session_ == nullptr) {
        return;
    }
    // The same shape the Java side logs and sends, so a line reads the same wherever it is read.
    const Component text = Component::text("<" + session_->name() + "> " + std::move(message));
    context_.logger.info([this, &text] { return wire::text::resolvePlainText(text, context_.lang); });
    announce(text);
}

void BedrockPlayer::move(const double x, const double y, const double z, const float yaw, const float pitch) {
    if (session_ == nullptr) {
        return;
    }
    // A client sends this every tick, so only the first one is worth a line: it says the movement
    // arrived and was read as a position, which is the part that can be wrong.
    if (!moved_) {
        moved_ = true;
        context_.logger.debug([this, x, y, z] {
            return session_->name() + " is moving: " + std::to_string(x) + ", " + std::to_string(y) + ", " +
                   std::to_string(z);
        });
    }
    // Written from the listener's thread, read by the tick, which is exactly what a Java connection
    // does with the same fields from its own thread.
    session_->x = x;
    session_->y = y;
    session_->z = z;
    session_->yaw = yaw;
    session_->pitch = pitch;
}

void BedrockPlayer::showMenu(const std::string& title, const std::map<std::int32_t, server::plugins::PluginItem>& items,
                             std::function<void(std::int32_t)> onClick) {
    std::vector<std::string> buttons;
    std::vector<std::int32_t> slots;
    buttons.reserve(items.size());
    slots.reserve(items.size());
    for (const auto& [slot, item] : items) {
        // What the plugin named it, flattened: a Bedrock button is a line of text and nothing else.
        const std::string label =
            item.name.has_value() ? wire::text::toLegacyText(wire::text::miniMessage::parse(*item.name)) : item.id;
        buttons.push_back(label.empty() ? item.id : label);
        slots.push_back(slot);
    }

    std::uint32_t formId = 0;
    {
        const std::lock_guard<std::mutex> guard{formLock_};
        formId = nextFormId_++;
        openTitle_ = title;
        form_ = OpenForm{formId, std::move(slots), std::move(onClick)};
    }
    wire::io::ByteBuffer body;
    wire::bedrock::writeModalFormRequest(
        body, formId,
        wire::bedrock::simpleFormJson(wire::text::toLegacyText(wire::text::miniMessage::parse(title)), "", buttons));
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ModalFormRequest), body.toVector());
}

void BedrockPlayer::formAnswer(const std::uint32_t formId, const std::optional<std::string>& data) {
    std::function<void(std::int32_t)> onClick;
    std::int32_t slot = -1;
    {
        const std::lock_guard<std::mutex> guard{formLock_};
        if (!form_.has_value() || form_->id != formId) {
            return;
        }
        if (!data.has_value()) {
            // Dismissed. The plugin hears nothing, which is what closing a window without clicking
            // is on the other edition too.
            form_.reset();
            return;
        }
        const std::int32_t button = std::atoi(data->c_str());
        if (button < 0 || static_cast<std::size_t>(button) >= form_->slots.size()) {
            return;
        }
        slot = form_->slots[static_cast<std::size_t>(button)];
        onClick = form_->onClick;
    }
    if (onClick) {
        onClick(slot);
    }
}

void BedrockPlayer::respawn() {
    if (session_ == nullptr || !session_->dead) {
        return;
    }
    session_->dead = false;
    session_->setHealth(session_->maxHealth());
    session_->food.set(player::FoodData::kMaxFood, player::FoodData::kDefaultSaturation);
    session_->x = context_.spawnX;
    session_->y = context_.spawnY;
    session_->z = context_.spawnZ;

    wire::io::ByteBuffer body;
    wire::bedrock::writeRespawn(
        body, static_cast<float>(session_->x), static_cast<float>(session_->y) + wire::bedrock::kPlayerEyeHeight,
        static_cast<float>(session_->z), wire::bedrock::RespawnState::ServerReady, kRuntimeEntityId);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Respawn), body.toVector());
    session_->connection().syncHealth();
}

void BedrockPlayer::tick() {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now - lastSync_ < kSyncPeriod) {
        return;
    }
    lastSync_ = now;
    syncOthers();

    // The world follows: crossing into another column is what asks for the ring that just came into
    // range, and nothing happens while somebody stands still.
    if (session_ != nullptr) {
        const std::pair here{static_cast<std::int32_t>(std::floor(session_->x)) >> 4,
                             static_cast<std::int32_t>(std::floor(session_->z)) >> 4};
        if (lastColumn_.has_value() && *lastColumn_ != here) {
            static_cast<void>(sendColumnsAround({}));
        }
    }
}

void BedrockPlayer::sendColumn(const std::int32_t chunkX, const std::int32_t chunkZ) {
    // Only a column this client already has: one it has never been sent arrives whole when they
    // walk into range, and sending it early would leave the client holding two of the same.
    if (!context_.sendChunks || !columns_.contains({chunkX, chunkZ})) {
        return;
    }
    context_.sendChunks({{chunkX, chunkZ}}, protocol_,
                        [mailbox = mailbox_](std::vector<std::uint8_t> column) {
                            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelChunk),
                                          std::move(column));
                        },
                        {});
}

bool BedrockPlayer::sendColumnsAround(std::function<void()> done) {
    if (session_ == nullptr || !context_.sendChunks) {
        return false;
    }
    const std::int32_t centreX = static_cast<std::int32_t>(std::floor(session_->x)) >> 4;
    const std::int32_t centreZ = static_cast<std::int32_t>(std::floor(session_->z)) >> 4;
    lastColumn_ = std::pair{centreX, centreZ};

    // What is in range now, minus what this client already has, and the far ones forgotten so that
    // walking back sends them again.
    std::set<std::pair<std::int32_t, std::int32_t>> inRange;
    std::vector<std::pair<std::int32_t, std::int32_t>> wanted;
    for (std::int32_t x = centreX - kChunkRadius; x <= centreX + kChunkRadius; ++x) {
        for (std::int32_t z = centreZ - kChunkRadius; z <= centreZ + kChunkRadius; ++z) {
            inRange.emplace(x, z);
            if (!columns_.contains({x, z})) {
                wanted.emplace_back(x, z);
            }
        }
    }
    columns_ = std::move(inRange);

    // Where the client should expect the world to be, which is what stops it waiting for columns
    // that are not coming.
    wire::io::ByteBuffer centre;
    wire::bedrock::writeNetworkChunkPublisherUpdate(centre, centreX * 16, static_cast<std::int32_t>(session_->y),
                                                    centreZ * 16, kChunkRadius * 16);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::NetworkChunkPublisherUpdate),
                   centre.toVector());

    if (wanted.empty()) {
        if (done) {
            done();
        }
        return true;
    }
    context_.sendChunks(
        std::move(wanted), protocol_,
        [mailbox = mailbox_](std::vector<std::uint8_t> column) {
            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelChunk), std::move(column));
        },
        std::move(done));
    return true;
}

void BedrockPlayer::syncOthers() {
    if (session_ == nullptr || context_.registry == nullptr) {
        return;
    }
    const auto post = [this](const wire::bedrock::BedrockPacket packet, wire::io::ByteBuffer& body) {
        mailbox_->post(static_cast<std::uint16_t>(packet), body.toVector());
    };

    std::unordered_map<wire::common::Uuid, Other> present;
    for (const player::PlayerSession* other : context_.registry->snapshot()) {
        if (other->uuid() == session_->uuid() || other->worldName != session_->worldName) {
            continue;
        }
        const Other now{other->entityId(), other->x, other->y, other->z, other->yaw, other->pitch};
        present.emplace(other->uuid(), now);

        const auto known = others_.find(other->uuid());
        if (known == others_.end()) {
            wire::bedrock::PlayerListEntry entry;
            entry.uuidHigh = other->uuid().high();
            entry.uuidLow = other->uuid().low();
            entry.entityId = static_cast<std::int64_t>(runtimeIdFor(now.entityId));
            entry.name = other->name();
            entry.skin = wire::bedrock::flatSkin(other->name());
            wire::io::ByteBuffer list;
            wire::bedrock::writePlayerListAdd(list, std::span{&entry, 1}, protocol_);
            post(wire::bedrock::BedrockPacket::PlayerList, list);

            wire::bedrock::AddPlayerInfo spawn;
            spawn.uuidHigh = entry.uuidHigh;
            spawn.uuidLow = entry.uuidLow;
            spawn.entityId = entry.entityId;
            spawn.runtimeEntityId = static_cast<std::uint64_t>(entry.entityId);
            spawn.name = entry.name;
            spawn.x = static_cast<float>(now.x);
            spawn.y = static_cast<float>(now.y);
            spawn.z = static_cast<float>(now.z);
            spawn.yaw = now.yaw;
            spawn.pitch = now.pitch;
            spawn.gameType = static_cast<std::int32_t>(other->gamemode);
            wire::io::ByteBuffer body;
            wire::bedrock::writeAddPlayer(body, spawn, protocol_);
            post(wire::bedrock::BedrockPacket::AddPlayer, body);
            continue;
        }

        const Other& before = known->second;
        const bool moved = std::abs(now.x - before.x) > kMoveEpsilon || std::abs(now.y - before.y) > kMoveEpsilon ||
                           std::abs(now.z - before.z) > kMoveEpsilon;
        const bool turned =
            std::abs(now.yaw - before.yaw) > kTurnEpsilon || std::abs(now.pitch - before.pitch) > kTurnEpsilon;
        if (!moved && !turned) {
            continue;
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeMoveEntity(body, runtimeIdFor(now.entityId), static_cast<float>(now.x),
                                       static_cast<float>(now.y) + wire::bedrock::kPlayerEyeHeight,
                                       static_cast<float>(now.z), now.yaw, now.pitch, true);
        post(wire::bedrock::BedrockPacket::MoveEntityAbsolute, body);
    }

    for (const auto& [uuid, gone] : others_) {
        if (present.contains(uuid)) {
            continue;
        }
        const std::pair<std::uint64_t, std::uint64_t> id{uuid.high(), uuid.low()};
        wire::io::ByteBuffer list;
        wire::bedrock::writePlayerListRemove(list, std::span{&id, 1}, protocol_);
        post(wire::bedrock::BedrockPacket::PlayerList, list);

        wire::io::ByteBuffer body;
        wire::bedrock::writeRemoveEntity(body, static_cast<std::int64_t>(runtimeIdFor(gone.entityId)));
        post(wire::bedrock::BedrockPacket::RemoveEntity, body);
    }
    others_ = std::move(present);
}

void BedrockPlayer::announce(const Component& message) const {
    if (context_.registry == nullptr) {
        return;
    }
    for (const player::PlayerSession* player : context_.registry->snapshot()) {
        if (player->connection().sendSystemMessage) {
            player->connection().sendSystemMessage(message);
        }
    }
}

} // namespace basalt::server::runtime::bedrock
