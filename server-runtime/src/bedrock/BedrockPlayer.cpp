#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <span>
#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

using wire::text::Component;

/// What `start_game` told this client it is. Every client is told the same number: a Bedrock
/// session only ever draws one player of its own.
constexpr std::uint64_t kRuntimeEntityId = 1;

/// How often the rest of the server is looked at, in the listener's own loop. The world moves once
/// every fifty milliseconds and the loop turns faster than that, so this is what a tick is worth.
constexpr std::chrono::milliseconds kSyncPeriod{50};

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
    : context_(std::move(context)), mailbox_(joining.mailbox) {
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
                                     yaw, pitch, 0);
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

    const std::int32_t entityId = context_.registry != nullptr ? context_.registry->allocateEntityId() : 1;
    session_ = std::make_unique<player::PlayerSession>(joining.uuid, joining.name, entityId, std::move(connection));
    session_->x = context_.spawnX;
    session_->y = context_.spawnY;
    session_->z = context_.spawnZ;
    session_->worldName = context_.worldName;

    if (context_.registry != nullptr) {
        context_.registry->add(session_.get());
    }
    // What the HUD draws, before anything can change it: a client that is told nothing draws a
    // player with no hearts at all.
    session_->connection().syncHealth();
    session_->connection().syncExperience();
    session_->connection().sendWeather(context_.weather);
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
    if (context_.quit) {
        context_.quit(*session_);
    }
    if (context_.registry != nullptr) {
        context_.registry->remove(session_->uuid());
    }
    announce(Component::translatable("multiplayer.player.left", {Component::text(session_->name())})
                 .color(wire::text::kYellow));
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

void BedrockPlayer::tick() {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now - lastSync_ < kSyncPeriod) {
        return;
    }
    lastSync_ = now;
    syncOthers();
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
            wire::bedrock::writePlayerListAdd(list, std::span{&entry, 1});
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
            wire::bedrock::writeAddPlayer(body, spawn);
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
        wire::bedrock::writePlayerListRemove(list, std::span{&id, 1});
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
