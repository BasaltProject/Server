#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/bedrock/BedrockMaps.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayerInternals.hpp>
#include <basalt/server/runtime/bedrock/BedrockRecipes.hpp>
#include <basalt/server/runtime/chat/ChatBroadcast.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerEntityData.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
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

/// How often the rest of the server is looked at, in the listener's own loop. The world moves once
/// every fifty milliseconds and the loop turns faster than that, so this is what a tick is worth.
constexpr std::chrono::milliseconds kSyncPeriod{50};

/// How far around a player the world is sent when they arrive. The same distance the door already
/// told the client to draw.
constexpr std::int32_t kChunkRadius = 4;

/// A position this client has already been told is the same position: a double that came off the
/// wire as a float never lands exactly where it started.
constexpr double kMoveEpsilon = 0.01;
constexpr float kTurnEpsilon = 1.0F;

/// The objective a server-drawn board lives under. One per client, so the name never collides.
constexpr std::string_view kSidebarObjective = "basalt";

} // namespace

std::uint64_t runtimeIdFor(const std::int32_t entityId) { return static_cast<std::uint64_t>(entityId) + 1; }

std::int64_t nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

wire::bedrock::BedrockItemStack itemStackFor(const std::optional<storage::StoredItemStack>& stack, const int protocol) {

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
            block = static_cast<std::int32_t>(wire::bedrock::bedrockBlockId(*state, protocol));
        }
    }
    wire::bedrock::BedrockItemStack item{
        .runtimeId = named->palettePlace + 1, .count = stack->count, .data = named->data, .blockRuntimeId = block};
    decorateMapStack(item, *stack);
    return item;
}

BedrockPlayer::BedrockPlayer(const BedrockListener::Joining& joining, Context context)
    : context_(std::move(context)), mailbox_(joining.mailbox), protocol_(joining.protocol), skins_(joining.skins) {
    player::PlayerConnection connection = buildConnection(joining);

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
    // The body itself, which nothing else says: gravity, collision and how much air it holds. Until
    // this arrives the client has zero for all three, which is a player who floats, cannot jump and
    // drowns standing on grass. Everywhere else it only goes out when the air or the fire changes.
    session_->connection().syncSubmersion();
    session_->connection().sendInventoryAll();
    // What its hands may do. Nothing else says this, and a client that is told nothing cannot mine,
    // build, open a door or hit anything whatever its gamemode is.
    sendAbilities();

    // Where they actually are. `start_game` went out before this player was loaded, so the client
    // was told the only place a start can name without knowing anybody: the origin. Left at that, a
    // client stands at zero sixty-four zero in a world where that is rock or water, and cannot jump
    // out of it because as far as it knows it is not standing anywhere.
    if (session_->connection().teleportTo) {
        session_->connection().teleportTo(session_->x, session_->y, session_->z, session_->yaw, session_->pitch);
    }

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
    announceAndSay(Component::translatable("multiplayer.player.joined", {Component::text(session_->name())})
                       .color(wire::text::kYellow));
    // The sidebar and the boss bar, off the same files and the same renderer the other door uses.
    if (context_.uiConfigs && context_.placeholders != nullptr) {
        ui_ = std::make_unique<ui::PlayerUi>(context_.uiConfigs, *context_.placeholders,
                                             [](std::int32_t, const std::function<void(wire::io::ByteBuffer&)>&) {
                                                 // Nothing speaks Java here: everything this player
                                                 // is shown goes out through the surfaces below.
                                             });
        ui::PlayerUi::Surfaces surfaces;
        surfaces.scoreboard = [this](const std::string& title, const std::vector<std::string>& lines) {
            drawSidebar(title, lines);
        };
        surfaces.clearScoreboard = [this] { clearSidebar(); };
        surfaces.bossBar = [this](const std::string& title, const float progress) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeBossEvent(body, static_cast<std::int64_t>(kRuntimeEntityId),
                                          bossBarShown_ ? wire::bedrock::BedrockBossAction::UpdatePercentage
                                                        : wire::bedrock::BedrockBossAction::Create,
                                          wire::text::toLegacyText(wire::text::miniMessage::parse(title)), progress,
                                          protocol_);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BossEvent), body.toVector());
            if (bossBarShown_) {
                // The percentage and the name are two separate nudges over there, and re-adding the
                // bar restarts its fade.
                wire::io::ByteBuffer named;
                wire::bedrock::writeBossEvent(
                    named, static_cast<std::int64_t>(kRuntimeEntityId), wire::bedrock::BedrockBossAction::UpdateName,
                    wire::text::toLegacyText(wire::text::miniMessage::parse(title)), progress, protocol_);
                mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BossEvent), named.toVector());
            }
            bossBarShown_ = true;
        };
        surfaces.clearBossBar = [this] {
            if (!bossBarShown_) {
                return;
            }
            wire::io::ByteBuffer body;
            wire::bedrock::writeBossEvent(body, static_cast<std::int64_t>(kRuntimeEntityId),
                                          wire::bedrock::BedrockBossAction::Remove, "", 0.0F, protocol_);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BossEvent), body.toVector());
            bossBarShown_ = false;
        };
        ui_->drawInto(std::move(surfaces));
    }

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
    // Whatever they were dragging goes back in the bag rather than leaving with the window. What
    // does not fit is lost, which can only happen if the slot it came from was filled again.
    if (windows_->cursor.has_value()) {
        static_cast<void>(session_->inventory.add(*windows_->cursor));
        windows_->cursor.reset();
    }
    // parity: `PlayerList.remove` awards it before the player is saved.
    player::statAwards::award(*session_, "leave_game");
    // Written before they leave the registry, so what is saved is a player who still exists.
    if (context_.save && session_->connection().storedSnapshot) {
        if (const std::optional<storage::StoredPlayer> row = session_->connection().storedSnapshot(); row.has_value()) {
            context_.save(*row);
        }
    }
    // parity: `ServerPlayer.disconnect` stops the sleeping, which is what puts the bed back down.
    // The world is not this thread's to touch, so the square goes over rather than the state.
    if (session_->sleepingBed.has_value() && context_.freeBed) {
        const std::array<std::int32_t, 3> bed = *session_->sleepingBed;
        session_->sleepingBed.reset();
        context_.freeBed(session_->worldName, bed[0], bed[1], bed[2]);
    }
    if (context_.quit) {
        context_.quit(*session_);
    }
    announceAndSay(Component::translatable("multiplayer.player.left", {Component::text(session_->name())})
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
    wire::bedrock::BedrockItemStack item{
        .runtimeId = named->palettePlace + 1, .count = stack->count, .data = named->data, .blockRuntimeId = block};
    decorateMapStack(item, *stack);
    return item;
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
    if (session_ == nullptr) {
        return;
    }
    const auto kind = static_cast<wire::bedrock::BedrockBlockActionType>(action);

    // A dig that is starting or carrying on: the client is told how fast it goes, which is both the
    // cracks and the pace. The world answers on its own thread, so the reply carries the mailbox.
    if (kind == wire::bedrock::BedrockBlockActionType::StartBreak ||
        kind == wire::bedrock::BedrockBlockActionType::ContinueBreak ||
        kind == wire::bedrock::BedrockBlockActionType::ContinueDestroy) {
        if (!context_.digSpeed) {
            return;
        }
        const bool starting = kind == wire::bedrock::BedrockBlockActionType::StartBreak;
        // The client says nothing about how far along it is, so the turns are counted here: it is
        // what everybody else's cracks are drawn from.
        if (starting || diggingAt_ != std::array<std::int32_t, 3>{x, y, z}) {
            diggingAt_ = {x, y, z};
            diggingTicks_ = 0;
        }
        ++diggingTicks_;
        context_.digSpeed(
            *session_, x, y, z, diggingTicks_, [mailbox = mailbox_, x, y, z, starting](const float perTick) {
                // Nothing to draw on a block that comes apart in one tick or not at all.
                if (perTick <= 0.0F || perTick >= 1.0F) {
                    return;
                }
                // How many whole ticks it takes, and the range shared out over them: the client counts in
                // ticks, so a block that needs three and a half of them needs four.
                const auto ticks = static_cast<std::int32_t>(std::ceil(1.0F / perTick));
                wire::io::ByteBuffer body;
                wire::bedrock::writeLevelEvent(body,
                                               starting ? wire::bedrock::LevelEventType::BlockStartBreak
                                                        : wire::bedrock::LevelEventType::BlockUpdateBreak,
                                               static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
                                               wire::bedrock::kBreakProgressScale / ticks);
                mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelEvent), body.toVector());
            });
        return;
    }

    // Given up on: the cracks come off again.
    if (kind == wire::bedrock::BedrockBlockActionType::AbortBreak) {
        if (diggingAt_.has_value() && context_.digStopped) {
            context_.digStopped(*session_, (*diggingAt_)[0], (*diggingAt_)[1], (*diggingAt_)[2]);
        }
        diggingAt_.reset();
        diggingTicks_ = 0;
        wire::io::ByteBuffer body;
        wire::bedrock::writeLevelEvent(body, wire::bedrock::LevelEventType::BlockStopBreak, static_cast<float>(x),
                                       static_cast<float>(y), static_cast<float>(z), 0);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::LevelEvent), body.toVector());
        return;
    }

    // A client that predicts the break has already taken the block away on its own screen, which is
    // the one that counts here: everything else is told by the world. Only this one action carries
    // where it happened; `stop_break` arrives at the origin, and breaking what is there instead is
    // how this server spent a while quarrying the middle of the world.
    if (!context_.breakBlock || kind != wire::bedrock::BedrockBlockActionType::PredictDestroy) {
        return;
    }
    context_.breakBlock(*session_, x, y, z);
}

void BedrockPlayer::useItemOn(const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t face,
                              const float clickX, const float clickY, const float clickZ,
                              const std::int32_t hotbarSlot) {
    if (session_ == nullptr || !context_.useOn || face < 0 || face > 5) {
        return;
    }
    // The two editions number the six faces the same way, down first and east last, so the number
    // travels as it arrives.
    const player::BlockHit hit{.x = x,
                               .y = y,
                               .z = z,
                               .direction = static_cast<wire::protocol::play::Direction>(face),
                               .clickX = clickX,
                               .clickY = clickY,
                               .clickZ = clickZ};
    // The answer comes back on the world's thread, where this player may already be gone: it
    // carries the mailbox rather than the player, which is what the goodbye leaves behind.
    context_.useOn(*session_, hit, hotbarSlot, windows_,
                   [mailbox = mailbox_, protocol = protocol_](
                       const OpenWindow& window, const std::vector<std::optional<storage::StoredItemStack>>& slots) {
                       drawWindow(mailbox, protocol, window, slots);
                   });
}

void drawWindowSlots(const std::shared_ptr<BedrockMailbox>& mailbox, const int protocol, const OpenWindow& window,
                     const std::vector<std::optional<storage::StoredItemStack>>& slots) {
    std::vector<wire::bedrock::BedrockItemStack> items;
    items.reserve(slots.size());
    for (const std::optional<storage::StoredItemStack>& stack : slots) {
        items.push_back(itemStackFor(stack, protocol));
    }
    wire::io::ByteBuffer body;
    wire::bedrock::writeInventoryContent(body, static_cast<std::uint32_t>(window.containerId), items,
                                         wire::bedrock::BedrockContainerSlot::LevelEntity, protocol);
    mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::InventoryContent), body.toVector());
}

void BedrockPlayer::drawWindow(const std::shared_ptr<BedrockMailbox>& mailbox, const int protocol,
                               const OpenWindow& window,
                               const std::vector<std::optional<storage::StoredItemStack>>& slots) {
    wire::io::ByteBuffer open;
    wire::bedrock::writeContainerOpen(open, window.containerId, window.type, window.x, window.y, window.z);
    mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ContainerOpen), open.toVector());
    drawWindowSlots(mailbox, protocol, window, slots);
}

void BedrockPlayer::setSneaking(const bool sneaking) {
    if (session_ != nullptr) {
        session_->sneaking = sneaking;
    }
}

void BedrockPlayer::setStance(const bool onGround, const bool sprinting) {
    if (session_ == nullptr) {
        return;
    }
    session_->onGround = onGround;
    session_->sprinting = sprinting;
}

void BedrockPlayer::setFlying(const bool flying) {
    if (session_ != nullptr) {
        session_->flying = flying && session_->mayFly();
    }
}

void BedrockPlayer::attack(const std::uint64_t runtimeEntityId) {
    if (session_ == nullptr || !context_.attackEntity || runtimeEntityId == 0) {
        return;
    }
    // The client counts entities from one where this server counts from zero.
    context_.attackEntity(*session_, static_cast<std::int32_t>(runtimeEntityId - 1), fallDistance_ > 0.0);
}

void BedrockPlayer::useOnEntity(const std::uint64_t runtimeEntityId) {
    if (session_ == nullptr || runtimeEntityId == 0) {
        return;
    }
    // The client counts entities from one where this server counts from zero.
    const auto entityId = static_cast<std::int32_t>(runtimeEntityId - 1);
    // parity: `Villager.mobInteract` opens its trades whatever is in the hand, so the book is asked
    // for first and the rest of the click only happens when there is none.
    if (context_.trades) {
        std::optional<OpenTrades> offers = context_.trades(*session_, entityId);
        if (offers.has_value() && !offers->offers.empty()) {
            openTrades(runtimeEntityId, *std::move(offers));
            return;
        }
    }
    if (context_.useEntity) {
        context_.useEntity(*session_, entityId);
    }
}

void BedrockPlayer::openTrades(const std::uint64_t runtimeEntityId, OpenTrades offers) {
    windows_->nextContainerId = windows_->nextContainerId >= kMaxContainerId ? 1 : windows_->nextContainerId + 1;
    // A merchant's window is not a block's: it carries no slots of this server's own, so nothing is
    // left open behind it and closing it has nothing to put back.
    windows_->open.reset();
    windows_->saidSlots.clear();
    windows_->saidNumbers.reset();
    windows_->trades = std::move(offers);
    tradingWith_ = runtimeEntityId;
    sendTrades();
}

void BedrockPlayer::sendTrades() {
    if (!windows_->trades.has_value() || windows_->trades->offers.empty() || tradingWith_ == 0) {
        return;
    }
    std::vector<wire::bedrock::BedrockTrade> lines;
    lines.reserve(windows_->trades->offers.size());
    for (const world::trades::Offer& offer : windows_->trades->offers) {
        const auto [costA, costB] = world::trades::currentCost(offer);
        wire::bedrock::BedrockTrade line;
        line.costA = tradeItemOf(costA);
        line.baseCountA = offer.costA.count;
        if (costB.has_value()) {
            line.costB = tradeItemOf(*costB);
            line.baseCountB = offer.costB->count;
        }
        line.sell = tradeItemOf(offer.result);
        line.maxUses = offer.maxUses;
        line.uses = offer.uses;
        line.experience = offer.experience;
        line.priceMultiplier = offer.priceMultiplier;
        line.demand = offer.demand;
        line.tier = windows_->trades->tier;
        lines.push_back(line);
    }
    wire::io::ByteBuffer body;
    wire::bedrock::writeUpdateTrade(body, windows_->nextContainerId, windows_->trades->tier,
                                    static_cast<std::int64_t>(tradingWith_),
                                    static_cast<std::int64_t>(kRuntimeEntityId), "Villager", lines);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateTrade), body.toVector());
}

void BedrockPlayer::sendEnchantOptions() {
    /// What the client draws in its own made-up alphabet. The words mean nothing: vanilla picks
    /// them at random out of a list nobody reads.
    constexpr std::string_view kRowName = "enchant";

    std::vector<wire::bedrock::BedrockEnchantOption> options;
    if (windows_->enchanting.has_value()) {
        for (const EnchantRow& row : windows_->enchanting->rows) {
            if (row.cost <= 0 || row.offers.empty()) {
                continue;
            }
            wire::bedrock::BedrockEnchantOption option;
            option.cost = row.cost;
            option.netId = row.netId;
            option.name = kRowName;
            for (const world::enchanting::Offer& offer : row.offers) {
                const std::optional<std::int32_t> type = wire::bedrock::bedrockEnchantment(offer.enchantment.path());
                if (type.has_value()) {
                    option.enchants.push_back({.type = *type, .level = offer.level});
                }
            }
            options.push_back(std::move(option));
        }
    }
    wire::io::ByteBuffer body;
    wire::bedrock::writePlayerEnchantOptions(body, options, protocol_);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlayerEnchantOptions), body.toVector());
}

void BedrockPlayer::editSign(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                             const std::string& text) {
    if (session_ == nullptr || !context_.writeSign) {
        return;
    }
    context_.writeSign(*session_, x, y, z, text);
}

void BedrockPlayer::editBookPage(const std::int32_t slot, const world::books::PageEdit what, const std::int32_t page,
                                 const std::int32_t other, const std::string& text) {
    if (session_ == nullptr || !context_.writeBookPage) {
        return;
    }
    context_.writeBookPage(*session_, slot, what, page, other, text);
}

void BedrockPlayer::signBook(const std::int32_t slot, const std::string& title) {
    if (session_ == nullptr || !context_.signBook) {
        return;
    }
    context_.signBook(*session_, slot, title);
}

void BedrockPlayer::requestMap(const std::int64_t mapId) {
    if (session_ != nullptr && context_.mapInfo) {
        context_.mapInfo(*session_, mapId);
    }
}

void BedrockPlayer::showBorderColliders(const double minX, const double maxX, const double minZ, const double maxZ,
                                        const std::int32_t warningBlocks) {
    /// The ids the two stand under. Negative on purpose: no entity of the world ever takes one.
    constexpr std::int64_t kColliderX = -3001;
    constexpr std::int64_t kColliderZ = -3002;
    /// How close to a face it is worth standing at, and how far below the player's feet it stands
    /// so that falling does not drop them through it.
    constexpr double kNearFace = 8.0;
    constexpr double kHalfWidth = 5.0;
    constexpr double kBelowFeet = 10.0;

    if (session_ == nullptr) {
        return;
    }
    const double warning = static_cast<double>(warningBlocks) + kNearFace;
    const auto standing = [this](const std::int64_t id, bool& up, const std::optional<std::array<double, 3>>& where) {
        if (!where.has_value()) {
            if (up) {
                wire::io::ByteBuffer body;
                wire::bedrock::writeRemoveEntity(body, id);
                mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::RemoveEntity), body.toVector());
                up = false;
            }
            return;
        }
        wire::io::ByteBuffer body;
        if (!up) {
            wire::bedrock::writeAddBorderCollider(body, id, static_cast<float>((*where)[0]),
                                                  static_cast<float>((*where)[1]), static_cast<float>((*where)[2]),
                                                  protocol_);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::AddEntity), body.toVector());
            up = true;
            return;
        }
        wire::bedrock::writeMoveEntity(body, static_cast<std::uint64_t>(id), static_cast<float>((*where)[0]),
                                       static_cast<float>((*where)[1]), static_cast<float>((*where)[2]), 0.0F, 0.0F,
                                       false);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::MoveEntityAbsolute), body.toVector());
    };
    // Nothing to walk into once they are already past it, which is where the damage takes over.
    const bool inside = session_->x > minX && session_->x < maxX && session_->z > minZ && session_->z < maxZ;
    const double standY = session_->y - kBelowFeet;
    std::optional<std::array<double, 3>> alongX;
    std::optional<std::array<double, 3>> alongZ;
    if (inside) {
        // Half its own width past the face, because the box is drawn around where it stands.
        if (session_->x > maxX - warning) {
            alongX = std::array{maxX + kHalfWidth, standY, session_->z};
        } else if (session_->x < minX + warning) {
            alongX = std::array{minX - kHalfWidth, standY, session_->z};
        }
        if (session_->z > maxZ - warning) {
            alongZ = std::array{session_->x, standY, maxZ + kHalfWidth};
        } else if (session_->z < minZ + warning) {
            alongZ = std::array{session_->x, standY, minZ - kHalfWidth};
        }
    }
    standing(kColliderX, borderColliderX_, alongX);
    standing(kColliderZ, borderColliderZ_, alongZ);
}

void BedrockPlayer::swing() {
    if (session_ != nullptr && context_.swing) {
        context_.swing(*session_);
    }
}

void BedrockPlayer::pickBlock(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                              const std::int32_t hotbarSlot) {
    if (session_ != nullptr && context_.pickBlock) {
        context_.pickBlock(*session_, x, y, z, hotbarSlot);
    }
}

void BedrockPlayer::pickEntity(const std::uint64_t runtimeEntityId) {
    if (session_ == nullptr || !context_.pickEntity || runtimeEntityId == 0) {
        return;
    }
    // The client counts entities from one where this server counts from zero.
    context_.pickEntity(*session_, static_cast<std::int32_t>(runtimeEntityId - 1));
}

void BedrockPlayer::turnLecternPage(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                    const std::int32_t page) {
    if (session_ != nullptr && context_.turnLecternPage) {
        context_.turnLecternPage(*session_, x, y, z, page);
    }
}

void BedrockPlayer::programCommandBlock(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                        const std::int32_t mode, const std::string& command, const bool trackOutput,
                                        const bool conditional, const bool needsRedstone) {
    if (session_ != nullptr && context_.programCommandBlock) {
        context_.programCommandBlock(*session_, x, y, z, mode, command, trackOutput, conditional, needsRedstone);
    }
}

void BedrockPlayer::toggleCrafterSlot(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                      const std::int32_t slot, const bool disabled) {
    if (session_ != nullptr && context_.toggleCrafterSlot) {
        context_.toggleCrafterSlot(*session_, x, y, z, slot, disabled);
    }
}

void BedrockPlayer::emote(const std::string& emoteId, const std::int32_t duration, const std::uint8_t flags) {
    if (session_ != nullptr && context_.emote) {
        context_.emote(*session_, emoteId, duration, flags);
    }
}

void BedrockPlayer::showEmote(const std::int32_t entityId, const std::string& emoteId, const std::int32_t duration,
                              const std::uint8_t flags) {
    const std::uint64_t runtime =
        session_ != nullptr && entityId == session_->entityId() ? kRuntimeEntityId : runtimeIdFor(entityId);
    wire::io::ByteBuffer body;
    wire::bedrock::writeEmote(body, runtime, emoteId, duration, flags, protocol_);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::Emote), body.toVector());
}

void BedrockPlayer::setHeldSlot(const std::int32_t slot) {
    if (session_ == nullptr || slot < 0 || slot >= player::PlayerInventory::kHotbarSize) {
        return;
    }
    session_->inventory.setHeldSlot(slot);
}

void BedrockPlayer::finishEating(const std::int32_t hotbarSlot) {
    if (session_ == nullptr || !context_.eat || hotbarSlot < 0 || hotbarSlot >= player::PlayerInventory::kHotbarSize) {
        return;
    }
    session_->usingItem = false;
    context_.eat(*session_, player::PlayerInventory::kHotbarStart + hotbarSlot);
}

void BedrockPlayer::useItemInAir(const std::int32_t hotbarSlot) {
    if (session_ == nullptr || hotbarSlot < 0 || hotbarSlot >= player::PlayerInventory::kHotbarSize) {
        return;
    }
    // Whatever it is, it is now being held down: a bow bending, a spyglass raised, a bottle tipped.
    drawingSince_ = std::chrono::steady_clock::now();
    session_->usingItem = true;
    if (context_.useInAir) {
        context_.useInAir(*session_, player::PlayerInventory::kHotbarStart + hotbarSlot);
    }
}

void BedrockPlayer::releaseItem(const std::int32_t hotbarSlot) {
    if (session_ == nullptr || hotbarSlot < 0 || hotbarSlot >= player::PlayerInventory::kHotbarSize) {
        return;
    }
    /// How long a tick is, which is what turns a client's own draw into the number a bow wants.
    constexpr std::int64_t kTickMillis = 50;

    const std::chrono::steady_clock::time_point started = drawingSince_.value_or(std::chrono::steady_clock::now());
    drawingSince_.reset();
    session_->usingItem = false;
    const auto held =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    if (context_.releaseItem) {
        context_.releaseItem(*session_, player::PlayerInventory::kHotbarStart + hotbarSlot,
                             static_cast<std::int32_t>(held / kTickMillis));
    }
}

void BedrockPlayer::openOwnInventory() {
    if (session_ == nullptr) {
        return;
    }
    // The bag itself is container zero, and it carries no block behind it: the position is the
    // player's own, which is what that client draws the screen over.
    wire::io::ByteBuffer body;
    wire::bedrock::writeContainerOpen(
        body, 0, wire::bedrock::BedrockContainerType::Inventory, static_cast<std::int32_t>(std::floor(session_->x)),
        static_cast<std::int32_t>(std::floor(session_->y)), static_cast<std::int32_t>(std::floor(session_->z)));
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ContainerOpen), body.toVector());
    // What is in it, so the screen it opens is not an empty one.
    if (session_->connection().sendInventoryAll) {
        session_->connection().sendInventoryAll();
    }
}

void BedrockPlayer::closeWindow(const std::int32_t containerId) {
    if (session_ == nullptr) {
        return;
    }
    if (context_.closeWindow) {
        context_.closeWindow(*session_, windows_);
    }
    // The client already took the screen down; this is the answer it waits for before it will open
    // another one.
    wire::io::ByteBuffer body;
    wire::bedrock::writeContainerClose(body, containerId, wire::bedrock::BedrockContainerType::Container, false);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ContainerClose), body.toVector());
}

void BedrockPlayer::stackRequest(const wire::bedrock::BedrockStackRequest& request) {
    if (session_ == nullptr || !context_.moveStacks) {
        return;
    }
    context_.moveStacks(*session_, request, windows_,
                        [mailbox = mailbox_, protocol = protocol_, requestId = request.requestId](
                            const bool accepted, const std::vector<wire::bedrock::BedrockResponseContainer>& windows) {
                            wire::io::ByteBuffer body;
                            wire::bedrock::writeItemStackResponse(body, requestId, accepted, windows, protocol);
                            mailbox->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::ItemStackResponse),
                                          body.toVector());
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
    // parity: a player with no chat session, whose line is unsigned chat like any other.
    if (context_.registry == nullptr) {
        return;
    }
    chat::broadcastChatMessage(
        context_.registry->snapshot(),
        chat::PlayerChatMessage::unsignedFrom(session_->uuid(), std::move(message), chat::nowMillis()),
        chat::BoundChatType{.type = chat::chatType::kChat, .name = chat::displayName(*session_, nullptr)},
        context_.logger, context_.lang);
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
    // How far they have dropped without landing. parity: `Entity.fallDistance`, which is what
    // decides whether a swing counts as a critical: only a hit on the way down does.
    if (session_->onGround) {
        fallDistance_ = 0.0;
    } else if (y < session_->y) {
        fallDistance_ += session_->y - y;
    }
    session_->fallDistance = fallDistance_;
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

void BedrockPlayer::showDialog(const world::dialogs::Dialog& dialog) {
    const auto flatten = [this](const wire::text::Component& text) {
        return wire::text::toLegacyText(wire::text::resolveComponent(text, context_.lang));
    };
    // The body is the whole of what a dialog says; a screen of buttons has one place to put it.
    std::string content;
    for (const wire::text::Component& line : dialog.body) {
        content += content.empty() ? flatten(line) : "\n\n" + flatten(line);
    }
    if (dialog.hasInputs) {
        content += content.empty() ? "" : "\n\n";
        content += "This dialog asks for something this edition cannot type here.";
    }

    // Held rather than referenced: the click comes back long after whoever asked for the dialog
    // has gone, and a button may lead to another dialog inside this one.
    const auto held = std::make_shared<const world::dialogs::Dialog>(dialog);
    std::vector<std::string> buttons;
    std::vector<std::int32_t> which;
    buttons.reserve(held->buttons.size());
    which.reserve(held->buttons.size());
    for (std::size_t at = 0; at < held->buttons.size(); ++at) {
        buttons.push_back(flatten(held->buttons[at].label));
        which.push_back(static_cast<std::int32_t>(at));
    }

    std::uint32_t formId = 0;
    {
        const std::lock_guard<std::mutex> guard{formLock_};
        formId = nextFormId_++;
        openTitle_.clear();
        form_ =
            OpenForm{formId, std::move(which), [this, held](const std::int32_t at) {
                         if (at < 0 || static_cast<std::size_t>(at) >= held->buttons.size()) {
                             return;
                         }
                         const world::dialogs::Button& pressed = held->buttons[static_cast<std::size_t>(at)];
                         switch (pressed.press) {
                         case world::dialogs::Press::RunCommand:
                             command(pressed.value);
                             return;
                         case world::dialogs::Press::ShowText:
                             if (session_ != nullptr && session_->connection().sendSystemMessage) {
                                 session_->connection().sendSystemMessage(wire::text::Component::text(pressed.value));
                             }
                             return;
                         case world::dialogs::Press::OpenDialog:
                             if (pressed.next != nullptr) {
                                 showDialog(*pressed.next);
                             }
                             return;
                         case world::dialogs::Press::None:
                             return;
                         }
                     }};
    }
    wire::io::ByteBuffer body;
    wire::bedrock::writeModalFormRequest(body, formId,
                                         wire::bedrock::simpleFormJson(flatten(dialog.title), content, buttons));
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

void BedrockPlayer::leaveBed() {
    if (session_ != nullptr && session_->connection().wakeUp) {
        session_->connection().wakeUp();
    }
}

void BedrockPlayer::settled(const bool afterDeath) {
    if (session_ == nullptr) {
        return;
    }
    if (afterDeath) {
        wire::io::ByteBuffer event;
        wire::bedrock::writeEntityEvent(event, kRuntimeEntityId, wire::bedrock::BedrockEntityEvent::Respawn, 0,
                                        protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::EntityEvent), event.toVector());
    } else {
        // Told again that it is in the world: a client that changed dimension sometimes never
        // decides so on its own and keeps the loading screen up.
        wire::io::ByteBuffer status;
        wire::bedrock::writePlayStatus(status, wire::bedrock::PlayStatus::PlayerSpawn);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::PlayStatus), status.toVector());
    }
    // Either way the attributes go again, because the client believes its own copy of them and a
    // screen it just took down is where that copy goes stale.
    if (session_->connection().syncHealth) {
        session_->connection().syncHealth();
    }
    if (session_->connection().syncExperience) {
        session_->connection().syncExperience();
    }
}

void BedrockPlayer::tick() {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now - lastSync_ < kSyncPeriod) {
        return;
    }
    lastSync_ = now;
    syncOthers();

    if (session_ != nullptr && context_.pickupItems) {
        context_.pickupItems(*session_);
    }
    if (ui_ != nullptr && session_ != nullptr) {
        const wire::common::placeholder::PlaceholderContext context =
            wire::common::placeholder::PlaceholderContext{}.with(runtime::placeholder::playerSlot(), session_.get());
        ui_->tick(uiTick_++, context);
    }
    syncSurroundings();
    tickCamera();
    if (session_ != nullptr && context_.lookAround) {
        // The answer lands on the world's thread, where this player may already be gone: it carries
        // the shared slot and nothing else.
        context_.lookAround(*session_, [into = surroundings_](std::vector<Seen> seen) {
            const std::lock_guard<std::mutex> guard{into->lock};
            into->latest = std::move(seen);
            into->fresh = true;
        });
    }

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

void BedrockPlayer::setCamera(const std::int32_t entityId) {
    if (session_ == nullptr) {
        return;
    }
    if (entityId == session_->entityId()) {
        if (spectating_ == 0) {
            return;
        }
        spectating_ = 0;
        clearCamera_ = true;
        wire::io::ByteBuffer body;
        wire::bedrock::writeCameraClear(body, true);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::CameraInstruction), body.toVector());
        return;
    }
    spectating_ = entityId;
    clearCamera_ = false;
    tickCamera();
}

void BedrockPlayer::tickCamera() {
    if (clearCamera_) {
        clearCamera_ = false;
        wire::io::ByteBuffer body;
        wire::bedrock::writeCameraClear(body, false);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::CameraInstruction), body.toVector());
    }
    if (spectating_ == 0) {
        return;
    }

    /// How far behind the watched entity the camera sits, and how much of their height the eyes
    /// are at. parity: Geyser's own two numbers.
    constexpr double kBehind = 4.0;
    constexpr double kEyeRatio = 0.85;
    constexpr double kDegrees = 0.017453292;
    /// Long enough to look smooth and short enough not to lag behind somebody running.
    constexpr float kEase = 0.15F;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double height = 1.8;
    float yaw = 0.0F;
    float pitch = 0.0F;
    bool found = false;
    for (const auto& [uuid, other] : others_) {
        if (other.entityId == spectating_) {
            x = other.x;
            y = other.y;
            z = other.z;
            yaw = other.yaw;
            pitch = other.pitch;
            found = true;
            break;
        }
    }
    if (!found) {
        const auto seen = shown_.find(spectating_);
        if (seen == shown_.end()) {
            // Out of range, or gone. The camera stays where it was rather than snapping home.
            return;
        }
        x = seen->second.x;
        y = seen->second.y;
        z = seen->second.z;
        yaw = seen->second.yaw;
        pitch = seen->second.pitch;
        height = static_cast<double>(seen->second.height);
        found = true;
    }

    // parity: `Mth`'s own view vector, which is what puts the camera on the line the eyes look
    // along rather than on the one the body faces.
    const double alongYaw = -static_cast<double>(yaw) * kDegrees;
    const double alongPitch = static_cast<double>(pitch) * kDegrees;
    const double flat = std::cos(alongPitch);
    const double forwardX = std::sin(alongYaw) * flat;
    const double forwardY = -std::sin(alongPitch);
    const double forwardZ = std::cos(alongYaw) * flat;

    const wire::bedrock::BedrockCamera camera{.preset = wire::bedrock::BedrockCameraPreset::Free,
                                              .x = static_cast<float>(x - (forwardX * kBehind)),
                                              .y = static_cast<float>(y + (height * kEyeRatio) - (forwardY * kBehind)),
                                              .z = static_cast<float>(z - (forwardZ * kBehind)),
                                              .pitch = pitch,
                                              .yaw = yaw,
                                              .easeSeconds = kEase};
    wire::io::ByteBuffer body;
    wire::bedrock::writeCameraSet(body, camera);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::CameraInstruction), body.toVector());
}

void BedrockPlayer::syncSurroundings() {
    std::vector<Seen> now;
    {
        const std::lock_guard<std::mutex> guard{surroundings_->lock};
        if (!surroundings_->fresh) {
            return;
        }
        surroundings_->fresh = false;
        now = std::move(surroundings_->latest);
    }

    /// How it is drawn, which this edition keeps in a handful of fields shared by every mob, and
    /// for the newest looks in a property of its own.
    const auto drawLook = [this](const std::int32_t id, const Seen& seen) {
        if (wire::bedrock::hasAnything(seen.look)) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeEntityLook(body, runtimeIdFor(id), seen.look, protocol_);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityData), body.toVector());
        }
        if (seen.look.climate.has_value()) {
            wire::io::ByteBuffer property;
            wire::bedrock::writeEntityIntProperty(property, runtimeIdFor(id), 0, *seen.look.climate);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityData),
                           property.toVector());
        }
    };

    /// An item frame, which is a block over here: the block itself and then what is in it.
    const auto drawFrame = [this](const Seen::Frame& frame) {
        wire::io::ByteBuffer block;
        wire::bedrock::writeItemFrameBlock(block, frame.x, frame.y, frame.z, frame.glowing, frame.face, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateBlock), block.toVector());
        wire::io::ByteBuffer inside;
        wire::bedrock::writeItemFrameData(inside, frame.x, frame.y, frame.z, frame.glowing, frame.item, frame.count,
                                          frame.rotation);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BlockEntityData), inside.toVector());
    };

    /// The line over an entity, which is its own packet here rather than a field of the spawn.
    const auto drawName = [this](const std::int32_t id, const Seen& seen) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeNameTag(body, runtimeIdFor(id), seen.name, seen.alwaysShowName, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetEntityData), body.toVector());
    };

    std::unordered_map<std::int32_t, Seen> present;
    for (Seen& seen : now) {
        const std::int32_t id = seen.entityId;
        const auto known = shown_.find(id);
        if (known == shown_.end()) {
            wire::io::ByteBuffer body;
            if (seen.stack.has_value()) {
                wire::bedrock::writeAddItemEntity(body, static_cast<std::int64_t>(runtimeIdFor(id)),
                                                  itemStackFor(seen.stack, protocol_), static_cast<float>(seen.x),
                                                  static_cast<float>(seen.y), static_cast<float>(seen.z), 0.0F, 0.0F,
                                                  0.0F, protocol_);
                mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::AddItemEntity),
                               body.toVector());
            } else if (seen.frame.has_value()) {
                drawFrame(*seen.frame);
                present.emplace(id, std::move(seen));
                continue;
            } else if (!seen.motive.empty()) {
                wire::bedrock::writeAddPainting(body, static_cast<std::int64_t>(runtimeIdFor(id)), runtimeIdFor(id),
                                                static_cast<float>(seen.x), static_cast<float>(seen.y),
                                                static_cast<float>(seen.z), seen.facing, seen.motive);
                mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::AddPainting), body.toVector());
                present.emplace(id, std::move(seen));
                continue;
            } else {
                wire::bedrock::writeAddEntity(body, static_cast<std::int64_t>(runtimeIdFor(id)), seen.type,
                                              static_cast<float>(seen.x), static_cast<float>(seen.y),
                                              static_cast<float>(seen.z), seen.yaw, seen.pitch, seen.width, seen.height,
                                              seen.flags, protocol_);
                mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::AddEntity), body.toVector());
            }
            if (!seen.name.empty()) {
                drawName(id, seen);
            }
            if (wire::bedrock::hasAnything(seen.look) || seen.look.climate.has_value()) {
                drawLook(id, seen);
            }
            present.emplace(id, std::move(seen));
            continue;
        }

        const Seen& before = known->second;
        const bool moved = std::abs(seen.x - before.x) > kMoveEpsilon || std::abs(seen.y - before.y) > kMoveEpsilon ||
                           std::abs(seen.z - before.z) > kMoveEpsilon;
        const bool turned =
            std::abs(seen.yaw - before.yaw) > kTurnEpsilon || std::abs(seen.pitch - before.pitch) > kTurnEpsilon;
        if (moved || turned) {
            wire::io::ByteBuffer body;
            wire::bedrock::writeMoveEntity(body, runtimeIdFor(id), static_cast<float>(seen.x),
                                           static_cast<float>(seen.y), static_cast<float>(seen.z), seen.yaw, seen.pitch,
                                           true);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::MoveEntityAbsolute),
                           body.toVector());
        }
        if (seen.frame.has_value() && seen.frame != before.frame) {
            drawFrame(*seen.frame);
        }
        if (before.name != seen.name || before.alwaysShowName != seen.alwaysShowName) {
            drawName(id, seen);
        }
        if (before.look != seen.look) {
            drawLook(id, seen);
        }
        present.emplace(id, std::move(seen));
    }

    for (const auto& [id, gone] : shown_) {
        if (present.contains(id)) {
            continue;
        }
        if (gone.frame.has_value()) {
            // The frame was a block, so taking it away is putting back what was really there,
            // which for anything a frame can hang in is air.
            wire::io::ByteBuffer empty;
            wire::bedrock::writeUpdateBlock(empty, gone.frame->x, gone.frame->y, gone.frame->z, 0, protocol_);
            mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateBlock), empty.toVector());
            continue;
        }
        wire::io::ByteBuffer body;
        wire::bedrock::writeRemoveEntity(body, static_cast<std::int64_t>(runtimeIdFor(id)));
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::RemoveEntity), body.toVector());
    }
    shown_ = std::move(present);
}

void BedrockPlayer::sendAbilities() {
    if (session_ == nullptr) {
        return;
    }
    const wire::protocol::play::Gamemode mode = session_->gamemode;
    // A mode that only looks cannot change the world, and over here that is not an ability: it is
    // a packet of its own, and the client believes it over anything the layer says.
    wire::io::ByteBuffer settings;
    wire::bedrock::writeUpdateAdventureSettings(settings, mode == wire::protocol::play::Gamemode::Adventure ||
                                                              mode == wire::protocol::play::Gamemode::Spectator);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateAdventureSettings),
                   settings.toVector());

    wire::io::ByteBuffer abilities;
    // Not an operator as far as this packet goes: what an operator may do with a command is decided
    // by the command tree and the groups, not here, and claiming it would hand a client the
    // operator's own buttons.
    wire::bedrock::writeUpdateAbilities(abilities, static_cast<std::int64_t>(kRuntimeEntityId), false,
                                        mode == wire::protocol::play::Gamemode::Creative);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateAbilities), abilities.toVector());
}

void BedrockPlayer::drawSidebar(const std::string& title, const std::vector<std::string>& lines) {
    const std::string drawn = wire::text::toLegacyText(wire::text::miniMessage::parse(title));
    if (!sidebarTitle_.has_value() || *sidebarTitle_ != drawn) {
        wire::io::ByteBuffer shown;
        wire::bedrock::writeSetDisplayObjective(shown, "sidebar", kSidebarObjective, drawn);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetDisplayObjective), shown.toVector());
        sidebarTitle_ = drawn;
    }

    // Only what changed, the way the other door does it: these boards carry coordinates and a
    // spinner, so every single tick has a different line in it somewhere.
    std::vector<wire::bedrock::BedrockScoreLine> changed;
    for (std::size_t at = 0; at < lines.size(); ++at) {
        std::string text = wire::text::toLegacyText(wire::text::miniMessage::parse(lines[at]));
        if (at < sidebarShown_.size() && sidebarShown_[at] == text) {
            continue;
        }
        // Highest at the top, which is how a sidebar is read on both editions.
        changed.push_back(wire::bedrock::BedrockScoreLine{.lineId = static_cast<std::int64_t>(at) + 1,
                                                          .name = std::move(text),
                                                          .score = static_cast<std::int32_t>(lines.size() - at)});
    }

    // A board that got shorter leaves its tail behind unless it is taken away by name.
    std::vector<std::int64_t> gone;
    for (std::size_t at = lines.size(); at < sidebarShown_.size(); ++at) {
        gone.push_back(static_cast<std::int64_t>(at) + 1);
    }
    if (!gone.empty()) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeRemoveScores(body, kSidebarObjective, gone, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetScore), body.toVector());
    }
    if (!changed.empty()) {
        wire::io::ByteBuffer body;
        wire::bedrock::writeSetScore(body, kSidebarObjective, changed, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetScore), body.toVector());
    }

    sidebarShown_.clear();
    sidebarShown_.reserve(lines.size());
    for (const std::string& line : lines) {
        sidebarShown_.push_back(wire::text::toLegacyText(wire::text::miniMessage::parse(line)));
    }
}

void BedrockPlayer::clearSidebar() {
    if (sidebarShown_.empty()) {
        return;
    }
    std::vector<std::int64_t> gone;
    gone.reserve(sidebarShown_.size());
    for (std::size_t at = 0; at < sidebarShown_.size(); ++at) {
        gone.push_back(static_cast<std::int64_t>(at) + 1);
    }
    wire::io::ByteBuffer body;
    wire::bedrock::writeRemoveScores(body, kSidebarObjective, gone, protocol_);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::SetScore), body.toVector());
    wire::io::ByteBuffer objective;
    wire::bedrock::writeRemoveObjective(objective, kSidebarObjective);
    mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::RemoveObjective), objective.toVector());
    sidebarShown_.clear();
    sidebarTitle_.reset();
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
    // An item frame is a block this client was told about and the world knows nothing of, so a
    // column sent again writes air over it: the ones in this column are put back.
    for (const auto& [id, seen] : shown_) {
        if (!seen.frame.has_value() || (seen.frame->x >> 4) != chunkX || (seen.frame->z >> 4) != chunkZ) {
            continue;
        }
        wire::io::ByteBuffer block;
        wire::bedrock::writeItemFrameBlock(block, seen.frame->x, seen.frame->y, seen.frame->z, seen.frame->glowing,
                                           seen.frame->face, protocol_);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::UpdateBlock), block.toVector());
        wire::io::ByteBuffer inside;
        wire::bedrock::writeItemFrameData(inside, seen.frame->x, seen.frame->y, seen.frame->z, seen.frame->glowing,
                                          seen.frame->item, seen.frame->count, seen.frame->rotation);
        mailbox_->post(static_cast<std::uint16_t>(wire::bedrock::BedrockPacket::BlockEntityData), inside.toVector());
    }
}

bool BedrockPlayer::sendColumnsAround(std::function<void()> done) {
    if (session_ == nullptr || !context_.sendChunks) {
        return false;
    }
    // TEMPORARY: a join with no world at all. A client that stays in a void read everything else
    // happily, and then the columns are the only thing left to blame.
    if (const char* const drop = std::getenv("BASALT_BEDROCK_DROP");
        drop != nullptr && std::string_view{drop}.find("chunks") != std::string_view::npos) {
        context_.logger.warn([] { return "Leaving out of the Bedrock join: chunks"; });
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
        const std::optional<storage::StoredItemStack> holding = other->inventory.heldItem();
        std::array<std::optional<storage::StoredItemStack>, player::PlayerInventory::kArmourSlots> wearing{};
        std::string wornKey;
        for (std::int32_t slot = 0; slot < player::PlayerInventory::kArmourSlots; ++slot) {
            wearing[static_cast<std::size_t>(slot)] = other->inventory.at(player::PlayerInventory::kArmourStart + slot);
            wornKey += wearing[static_cast<std::size_t>(slot)].has_value() ? wearing[static_cast<std::size_t>(slot)]->id
                                                                           : std::string{};
            wornKey += '|';
        }
        // parity: what the other door carries in `SharedFlags` and the living flags, as this edition
        // draws them. Sleeping is a flag of the second word and has its own packet here.
        const player::PlayerEntityData data = player::PlayerEntityData::of(*other);
        const auto shared = [&data](const std::uint8_t bit) { return (data.sharedFlags & bit) != 0; };
        std::uint64_t flags = 0;
        if (shared(wire::protocol::play::SetEntityDataPacket::kFlagOnFire)) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::OnFire);
        }
        if (shared(wire::protocol::play::SetEntityDataPacket::kFlagSneaking)) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Sneaking);
        }
        if (shared(wire::protocol::play::SetEntityDataPacket::kFlagSprinting)) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Sprinting);
        }
        if (shared(wire::protocol::play::SetEntityDataPacket::kFlagSwimming)) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Swimming);
        }
        if (shared(wire::protocol::play::SetEntityDataPacket::kFlagInvisible)) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Invisible);
        }
        if (shared(wire::protocol::play::SetEntityDataPacket::kFlagFallFlying)) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Gliding);
        }
        if ((data.livingFlags & player::PlayerEntityData::kLivingFlagUsingItem) != 0) {
            flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::UsingItem);
        }
        const Other now{other->entityId(),
                        other->x,
                        other->y,
                        other->z,
                        other->yaw,
                        other->pitch,
                        holding.has_value() ? holding->id : std::string{},
                        wornKey,
                        flags};
        present.emplace(other->uuid(), now);

        // What is in their hand, whenever it is not what it was. Sent after the spawn below on the
        // first turn, because the client has nobody to put it in the hand of before that.
        const auto drawHand = [this, &post, &now, &holding] {
            wire::io::ByteBuffer body;
            wire::bedrock::writeMobEquipment(body, runtimeIdFor(now.entityId), itemStackFor(holding, protocol_), 0,
                                             protocol_);
            post(wire::bedrock::BedrockPacket::MobEquipment, body);
        };
        // Head, chest, legs and feet, in the order this edition reads them. The fifth is a mob's
        // own body plate, which a player never wears.
        const auto drawArmour = [this, &post, &now, &wearing] {
            wire::io::ByteBuffer body;
            wire::bedrock::writeMobArmorEquipment(
                body, runtimeIdFor(now.entityId), itemStackFor(wearing[0], protocol_),
                itemStackFor(wearing[1], protocol_), itemStackFor(wearing[2], protocol_),
                itemStackFor(wearing[3], protocol_), wire::bedrock::BedrockItemStack{}, protocol_);
            post(wire::bedrock::BedrockPacket::MobArmorEquipment, body);
        };

        /// How they are drawn, whenever that changed.
        const auto drawFlags = [this, &post, &now] {
            wire::io::ByteBuffer body;
            wire::bedrock::writeEntityFlags(body, runtimeIdFor(now.entityId), now.flags, protocol_);
            post(wire::bedrock::BedrockPacket::SetEntityData, body);
        };

        const auto known = others_.find(other->uuid());
        if (known == others_.end()) {
            wire::bedrock::PlayerListEntry entry;
            entry.uuidHigh = other->uuid().high();
            entry.uuidLow = other->uuid().low();
            entry.entityId = static_cast<std::int64_t>(runtimeIdFor(now.entityId));
            entry.name = other->name();
            // What that player's own client said it looks like, and a plain colour for anybody who
            // arrived through the other door and has no Bedrock skin to send.
            const std::optional<wire::bedrock::SkinInfo> theirs =
                skins_ == nullptr ? std::nullopt : skins_->of(other->uuid());
            entry.skin = theirs.has_value() ? *theirs : wire::bedrock::flatSkin(other->name());
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
            drawHand();
            drawArmour();
            if (now.flags != 0) {
                drawFlags();
            }
            continue;
        }

        const Other& before = known->second;
        if (before.held != now.held) {
            drawHand();
        }
        if (before.worn != now.worn) {
            drawArmour();
        }
        if (before.flags != now.flags) {
            drawFlags();
        }
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

void BedrockPlayer::announceAndSay(const Component& message) const {
    // On the console too, exactly as the Java side says it: joining and leaving are what an operator
    // watches the console for, and an edition is not a reason to stay quiet. Chat says itself where
    // it is read, so it does not come through here.
    wire::common::Logger{"Server"}.info(
        [this, &message] { return wire::text::resolvePlainText(message, context_.lang); });
    announce(message);
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
