#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayerInternals.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
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

#include <array>
#include <chrono>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

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
    connection.sendLevelEvent = [mailbox = joining.mailbox, protocol = joining.protocol](
                                    const std::int32_t type, const std::int32_t x, const std::int32_t y,
                                    const std::int32_t z, const std::int32_t data) {
        const wire::bedrock::BedrockLevelEffect effect = wire::bedrock::bedrockLevelEffect(type, data);
        if (effect.event != 0) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeLevelEvent(body, effect.event, static_cast<float>(x) + effect.x,
                                           static_cast<float>(y) + effect.y, static_cast<float>(z) + effect.z,
                                           effect.keepsData ? data : 0);
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
        // a table's grid is the player's, and it only moves when they move it.
        if (!windows->open.has_value() || windows->open->size == 0) {
            return;
        }
        // What it holds, for the turns nobody touched it: a furnace fills its own result slot, and
        // somebody else may be in the same chest. Not every turn: see `sinceRead`.
        if (++windows->sinceRead >= kWindowReadTurns) {
            windows->sinceRead = 0;
            std::vector<std::optional<storage::StoredItemStack>> slots = readWindow(world, *windows->open);
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
    return connection;
}

} // namespace basalt::server::runtime::bedrock
