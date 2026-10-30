#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/wire/bedrock/BedrockMaps.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>

#include <chrono>
#include <cstdlib>
#include <set>
#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

[[nodiscard]] std::string printable(const std::string_view said) {
    constexpr std::size_t kMaxLength = 100;
    std::string out;
    for (std::size_t at = 0; at < said.size(); ++at) {
        const auto letter = static_cast<unsigned char>(said[at]);
        if (letter == 0xc2U && at + 2 < said.size() && static_cast<unsigned char>(said[at + 1]) == 0xa7U) {
            at += 2;
            continue;
        }
        if (letter >= 32 && letter <= 126) {
            out.push_back(said[at]);
        }
        if (out.size() == kMaxLength) {
            out += "...";
            break;
        }
    }
    return out;
}

[[nodiscard]] std::string stackActionName(const wire::bedrock::BedrockStackActionType type) {
    switch (type) {
    case wire::bedrock::BedrockStackActionType::Take:
        return "take";
    case wire::bedrock::BedrockStackActionType::Place:
        return "place";
    case wire::bedrock::BedrockStackActionType::Swap:
        return "swap";
    case wire::bedrock::BedrockStackActionType::Drop:
        return "drop";
    case wire::bedrock::BedrockStackActionType::Destroy:
        return "destroy";
    case wire::bedrock::BedrockStackActionType::Consume:
        return "consume";
    case wire::bedrock::BedrockStackActionType::Create:
        return "create";
    case wire::bedrock::BedrockStackActionType::CraftResults:
        return "craft results";
    case wire::bedrock::BedrockStackActionType::Craft:
        return "craft";
    case wire::bedrock::BedrockStackActionType::CraftOptional:
        return "craft optional";
    case wire::bedrock::BedrockStackActionType::CraftGrindstone:
        return "craft grindstone";
    case wire::bedrock::BedrockStackActionType::BeaconPayment:
        return "beacon payment";
    case wire::bedrock::BedrockStackActionType::CraftLoom:
        return "craft loom";
    case wire::bedrock::BedrockStackActionType::Other:
        break;
    }
    return "one this build does not act on";
}

using wire::bedrock::BedrockConnection;
using wire::bedrock::BedrockPacket;
using wire::bedrock::RakSession;

// Twenty milliseconds: short enough that an acknowledgement is never late, long enough not to spin when idle.
constexpr int kPollTimeoutMillis = 20;

constexpr std::int32_t kInitialRadius = 4;

// A join glued every column into one batch of over a megabyte; sending a turn's worth at a time keeps pieces readable.
constexpr std::size_t kTurnBytes = 64U * 1024U;

constexpr std::size_t kLargePacketBytes = 1024;

[[nodiscard]] std::uint64_t nowMillis() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// A semicolon separates the fields of the line a client reads, so one in the message would shift the rest.
[[nodiscard]] std::string oneLine(std::string_view text) {
    std::string clean;
    clean.reserve(text.size());
    for (const char character : text) {
        clean += (character == ';' || character == '\n' || character == '\r') ? ' ' : character;
    }
    while (!clean.empty() && clean.front() == ' ') {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && clean.back() == ' ') {
        clean.pop_back();
    }
    return clean;
}

[[nodiscard]] std::pair<std::string, std::string> splitMotd(std::string_view text) {
    const std::size_t newline = text.find('\n');
    if (newline == std::string_view::npos) {
        return {oneLine(text), std::string{}};
    }
    return {oneLine(text.substr(0, newline)), oneLine(text.substr(newline + 1))};
}

} // namespace

namespace {

class RakChannel final : public BedrockListener::Channel {
public:
    explicit RakChannel(BedrockConnection& connection) : connection_(connection) {}

    [[nodiscard]] int protocolVersion() const override { return connection_.protocolVersion(); }

    void sendPacket(BedrockPacket packet, std::span<const std::uint8_t> body) override {
        connection_.sendPacket(packet, body);
    }

    void flush() override { connection_.flush(); }

    void close() override { connection_.close(); }

private:
    BedrockConnection& connection_;
};

} // namespace

struct BedrockListener::Session {
    RakSession* transport = nullptr;
    std::optional<BedrockConnection> rak;
    std::unique_ptr<Channel> link;
    std::string address;
    std::shared_ptr<BedrockMailbox> mailbox = std::make_shared<BedrockMailbox>();
    std::unique_ptr<BedrockListener::Player> player;
    wire::bedrock::LoginRequest login;
    bool started = false;
    /// TEMPORARY: said once per client, not once per packet.
    bool saidBadInput = false;
    // Packet kinds already seen ignored, so the log names each once rather than every tick.
    std::set<std::uint16_t> unanswered;

    /// Packets of a client a proxy carries, handed over by whichever thread reads that connection.
    std::mutex inboxLock;
    std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>> inbox;
    std::atomic<bool> ended{false};

    Session(RakSession& session, std::shared_ptr<const wire::bedrock::EcKeyPair> keys, std::string gameVersion,
            wire::bedrock::BedrockGate gate)
        : transport(&session), address(session.peer().toString()) {
        rak.emplace(session, std::move(keys), std::move(gameVersion), std::move(gate));
        link = std::make_unique<RakChannel>(*rak);
    }

    Session(std::unique_ptr<Channel> channel, std::string from) : link(std::move(channel)), address(std::move(from)) {}
};

void BedrockListener::Skins::remember(const wire::common::Uuid& who, wire::bedrock::SkinInfo skin) {
    const std::lock_guard<std::mutex> guard{lock_};
    skins_.insert_or_assign(who, std::move(skin));
}

void BedrockListener::Skins::forget(const wire::common::Uuid& who) {
    const std::lock_guard<std::mutex> guard{lock_};
    skins_.erase(who);
}

std::optional<wire::bedrock::SkinInfo> BedrockListener::Skins::of(const wire::common::Uuid& who) const {
    const std::lock_guard<std::mutex> guard{lock_};
    const auto found = skins_.find(who);
    return found == skins_.end() ? std::nullopt : std::optional{found->second};
}

void BedrockMailbox::post(const std::uint16_t packetId, std::vector<std::uint8_t> body, const bool last) {
    const std::lock_guard<std::mutex> guard{lock_};
    if (!open_) {
        return;
    }
    if (last) {
        // Ahead of everything waiting, and bounded: a goodbye behind a world of columns would arrive too late.
        items_.clear();
    }
    items_.push_back(Item{packetId, std::move(body), last});
    if (last) {
        open_ = false;
    }
}

std::vector<BedrockMailbox::Item> BedrockMailbox::drain(const std::size_t maxBytes) {
    const std::lock_guard<std::mutex> guard{lock_};
    std::size_t taken = 0;
    std::size_t count = 0;
    while (count < items_.size() && taken < maxBytes) {
        taken += items_[count].body.size();
        ++count;
    }
    if (count == items_.size()) {
        return std::exchange(items_, {});
    }
    std::vector<Item> some{std::make_move_iterator(items_.begin()),
                           std::make_move_iterator(items_.begin() + static_cast<std::ptrdiff_t>(count))};
    items_.erase(items_.begin(), items_.begin() + static_cast<std::ptrdiff_t>(count));
    return some;
}

void BedrockMailbox::close() {
    const std::lock_guard<std::mutex> guard{lock_};
    open_ = false;
    items_.clear();
}

BedrockListener::BedrockListener(BedrockOptions options, Hooks hooks, std::string gameVersion,
                                 wire::common::Logger logger)
    : options_(std::move(options)), hooks_(std::move(hooks)), gameVersion_(std::move(gameVersion)),
      logger_(std::move(logger)) {}

BedrockListener::~BedrockListener() { stop(); }

std::string BedrockListener::prefixedName(std::string_view gamertag) const {
    return options_.prefix + std::string{gamertag};
}

wire::common::Uuid BedrockListener::uuidOf(std::string_view xuid) {
    // Prefixed so this space cannot meet the offline uuids vanilla derives from a name.
    return wire::common::Uuid::nameBased("BedrockPlayer:" + std::string{xuid});
}

std::string BedrockListener::advertisement() const {
    const Status status = hooks_.status ? hooks_.status() : Status{"Basalt", 0, 20};
    const auto [motd, subMotd] = splitMotd(status.motd);

    // The field order is the client's, not ours.
    std::string line = "MCPE;";
    line += motd + ";";
    line += std::to_string(wire::bedrock::kNativeBedrockProtocol) + ";";
    line += gameVersion_ + ";";
    line += std::to_string(status.online) + ";";
    line += std::to_string(status.max) + ";";
    line += std::to_string(server_ ? server_->guid() : 0) + ";";
    line += subMotd + ";";
    line += "Survival;1;";
    line += std::to_string(port()) + ";";
    line += std::to_string(port()) + ";";
    return line;
}

bool BedrockListener::start() {
    if (!options_.enabled) {
        logger_.info([] { return "Bedrock is off in server.toml."; });
        return false;
    }

    auto keys = wire::bedrock::EcKeyPair::generate();
    if (!keys) {
        logger_.warn([] { return "Could not make the key pair Bedrock logins need; the door stays shut."; });
        return false;
    }
    keys_ = std::make_shared<const wire::bedrock::EcKeyPair>(std::move(*keys));

    wire::bedrock::RakServerConfig config;
    config.port = options_.port;
    server_ = std::make_unique<wire::bedrock::RakServer>(config);
    if (const auto started = server_->start(); !started) {
        logger_.warn([this] {
            return "Could not bind UDP port " + std::to_string(options_.port) +
                   " for Bedrock. Something else is on it, which is usually the last run of this "
                   "server still letting go of it: try again, or change `port` under "
                   "[network.bedrock] in config/server.toml.";
        });
        server_.reset();
        return false;
    }

    server_->onAdvertise([this] { return advertisement(); });
    server_->onConnect([this](RakSession& session) { onConnect(session); });
    server_->onMessage(
        [this](RakSession& session, std::span<const std::uint8_t> payload) { onMessage(session, payload); });
    server_->onDisconnect([this](RakSession& session) { onDisconnect(session); });

    running_.store(true);
    thread_ = std::jthread{[this] { run(); }};
    return true;
}

std::string BedrockListener::welcome() const {
    std::string protocols;
    for (const std::int32_t protocol : options_.gate.supported()) {
        if (!protocols.empty()) {
            protocols += ", ";
        }
        protocols += std::to_string(protocol);
    }
    return "Bedrock on UDP port " + std::to_string(port()) + " (" + gameVersion_ + ", protocol " + protocols + ")";
}

void BedrockListener::closeDoor() { accepting_.store(false); }

void BedrockListener::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    // One last turn of the post: goodbyes queued after the thread stopped are still here.
    for (const auto& [peer, session] : sessions_) {
        deliverMail(*session);
    }
    // And one turn of the socket: everything above only queues, and bytes reach the network inside the poll.
    if (server_ != nullptr) {
        server_->poll(nowMillis(), 0);
    }
    sessions_.clear();
    server_.reset();
}

std::uint16_t BedrockListener::port() const noexcept { return server_ ? server_->port() : options_.port; }

std::size_t BedrockListener::sessionCount() const { return sessions_.size(); }

void BedrockListener::run() {
    while (running_.load()) {
        // Before the poll: it flushes what is queued now, so another thread's line goes out this turn.
        const bool open = accepting_.load();
        for (const auto& [peer, session] : sessions_) {
            // After the door closes the server is saving these players: only the post runs, so goodbyes arrive.
            if (session->player != nullptr && open) {
                session->player->tick();
            }
            deliverMail(*session);
        }
        serveCarried();
        server_->poll(nowMillis(), kPollTimeoutMillis);
    }
}

namespace {

class CarriedHandle final : public BedrockListener::Carried {
public:
    CarriedHandle(std::function<void(std::span<const std::uint8_t>)> onPacket, std::function<void()> onEnd)
        : onPacket_(std::move(onPacket)), onEnd_(std::move(onEnd)) {}

    void receive(std::span<const std::uint8_t> packet) override { onPacket_(packet); }

    void end() override { onEnd_(); }

private:
    std::function<void(std::span<const std::uint8_t>)> onPacket_;
    std::function<void()> onEnd_;
};

} // namespace

std::shared_ptr<BedrockListener::Carried> BedrockListener::carry(const Identity& who,
                                                                 std::unique_ptr<Channel> channel) {
    if (!running_.load() || !accepting_.load()) {
        return nullptr;
    }
    auto session = std::make_shared<Session>(std::move(channel), who.address);
    session->login.name = who.gamertag;
    session->login.xuid = who.xuid;
    session->login.authenticated = who.authenticated;
    session->login.skinId = who.skinId;
    session->login.skinGeometry = who.resourcePatch;
    session->login.skinWidth = who.skinWidth;
    session->login.skinHeight = who.skinHeight;
    session->login.skinPixels = who.skinPixels;
    logger_.info([this, &who] {
        return prefixedName(who.gamertag) + " is joining from Bedrock through a proxy" +
               (who.authenticated ? "" : " (unauthenticated)") + ".";
    });
    {
        const std::lock_guard<std::mutex> guard{carriedLock_};
        carried_.push_back(session);
    }
    return std::make_shared<CarriedHandle>(
        [session](std::span<const std::uint8_t> packet) {
            wire::io::ByteBuffer reader;
            reader.writeBytes(packet);
            const std::uint32_t header = wire::bedrock::readVarUInt(reader);
            if (reader.failed()) {
                return;
            }
            constexpr std::uint32_t kPacketIdMask = 0x3ffU;
            const std::span<const std::uint8_t> body = reader.readable();
            const std::lock_guard<std::mutex> guard{session->inboxLock};
            session->inbox.emplace_back(static_cast<std::uint16_t>(header & kPacketIdMask),
                                        std::vector<std::uint8_t>(body.begin(), body.end()));
        },
        [session] { session->ended.store(true); });
}

void BedrockListener::serveCarried() {
    std::vector<std::shared_ptr<Session>> serving;
    {
        const std::lock_guard<std::mutex> guard{carriedLock_};
        std::erase_if(carried_, [](const std::shared_ptr<Session>& one) { return one->ended.load(); });
        serving = carried_;
    }
    const bool open = accepting_.load();
    for (const std::shared_ptr<Session>& session : serving) {
        if (!session->started) {
            onReady(*session);
        }
        std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>> arrived;
        {
            const std::lock_guard<std::mutex> guard{session->inboxLock};
            arrived.swap(session->inbox);
        }
        for (auto& [id, packet] : arrived) {
            wire::io::ByteBuffer body;
            body.writeBytes(packet);
            onGamePacket(*session, static_cast<BedrockPacket>(id), body);
        }
        if (session->player != nullptr && open) {
            session->player->tick();
        }
        deliverMail(*session);
    }
}

void BedrockListener::onConnect(RakSession& session) {
    // Shutting down: letting one in means a player who joins mid-save and is never saved.
    if (!accepting_.load()) {
        return;
    }
    auto owned = std::make_unique<Session>(session, keys_, gameVersion_, options_.gate);
    Session& entry = *owned;
    sessions_.emplace(session.peer(), std::move(owned));

    entry.rak->onLogin([this, &entry](const wire::bedrock::LoginRequest& login) {
        entry.login = login;
        logger_.info([this, &login] {
            return prefixedName(login.name) + " is joining from Bedrock" +
                   (login.authenticated ? "" : " (unauthenticated)") + ".";
        });
    });
    entry.rak->onRefused([this](const int protocol, const std::string& told) {
        logger_.info([protocol, &told] {
            return "A Bedrock client on protocol " + std::to_string(protocol) + " was turned away: " + told;
        });
    });
    entry.rak->onWarning([this](const int protocol) {
        logger_.warn([protocol] {
            return "A Bedrock client on protocol " + std::to_string(protocol) +
                   " is newer than this server speaks and was let in anyway; it may not get far.";
        });
    });
    entry.rak->onReady([this, &entry] { onReady(entry); });
    entry.rak->onPacket(
        [this, &entry](BedrockPacket packet, wire::io::ByteBuffer& body) { onGamePacket(entry, packet, body); });
}

void BedrockListener::onGamePacket(Session& entry, BedrockPacket packet, wire::io::ByteBuffer& body) {
    if (packet == BedrockPacket::RequestChunkRadius) {
        const auto radius = wire::bedrock::readRequestChunkRadius(body);
        if (!radius) {
            return;
        }
        wire::io::ByteBuffer answer;
        wire::bedrock::writeChunkRadiusUpdated(answer, *radius);
        entry.link->sendPacket(BedrockPacket::ChunkRadiusUpdated, answer.readable());
        entry.link->flush();
        return;
    }
    if (packet == BedrockPacket::PlayerAuthInput && entry.player) {
        const auto input = wire::bedrock::readPlayerAuthInput(body, entry.link->protocolVersion());
        if (!input.has_value() && !entry.saidBadInput) {
            entry.saidBadInput = true;
            logger_.warn([&entry] {
                return "Could not read player_auth_input from a client on protocol " +
                       std::to_string(entry.link->protocolVersion()) + ".";
            });
        }
        if (input.has_value()) {
            entry.player->setSneaking(input->sneaking);
            entry.player->setStance(input->onGround, input->sprinting);
            if (input->flying.has_value()) {
                entry.player->setFlying(*input->flying);
            }
            if (input->itemUse.has_value()) {
                logger_.debug([&input] {
                    return "item use " + std::to_string(input->itemUse->action) + " at " +
                           std::to_string(input->itemUse->x) + ", " + std::to_string(input->itemUse->y) + ", " +
                           std::to_string(input->itemUse->z) + " face " + std::to_string(input->itemUse->face) +
                           " slot " + std::to_string(input->itemUse->hotbarSlot);
                });
            }
            if (input->itemUse.has_value() && input->itemUse->action == 0) {
                entry.player->useItemOn(input->itemUse->x, input->itemUse->y, input->itemUse->z, input->itemUse->face,
                                        input->itemUse->clickX, input->itemUse->clickY, input->itemUse->clickZ,
                                        input->itemUse->hotbarSlot);
            }
            constexpr std::int32_t kUseInAir = 1;

            if (input->itemUse.has_value() && input->itemUse->action == kUseInAir) {
                entry.player->useItemInAir(input->itemUse->hotbarSlot);
            }
            if (input->stackRequest.has_value()) {
                logger_.debug([&input] {
                    std::string lines;
                    for (const wire::bedrock::BedrockStackAction& action : input->stackRequest->actions) {
                        lines += (lines.empty() ? "" : ", ") + stackActionName(action.type) + " (" +
                                 std::to_string(action.line) + ")";
                    }
                    return "stack request " + std::to_string(input->stackRequest->requestId) + ": " + lines;
                });
                entry.player->stackRequest(*input->stackRequest);
            }
            for (const wire::bedrock::BedrockBlockAction& action : input->blockActions) {
                logger_.debug([&action] {
                    return "block action " + std::to_string(action.action) + " at " + std::to_string(action.x) + ", " +
                           std::to_string(action.y) + ", " + std::to_string(action.z);
                });
                entry.player->blockAction(action.action, action.x, action.y, action.z, action.face);
            }
            entry.player->move(static_cast<double>(input->x),
                               static_cast<double>(input->y) - static_cast<double>(wire::bedrock::kPlayerEyeHeight),
                               static_cast<double>(input->z), input->yaw, input->pitch);
        }
        return;
    }
    if (packet == BedrockPacket::InventoryTransaction && entry.player) {
        constexpr std::int32_t kAttack = 1;

        constexpr std::int32_t kConsume = 1;

        const wire::bedrock::BedrockTransaction transaction =
            wire::bedrock::readTransaction(body, entry.link->protocolVersion());
        // A finished mouthful arrives like a hit: the client eats on its own clock and this is the only word of it.
        if (transaction.itemRelease.has_value()) {
            if (transaction.itemRelease->action == kConsume) {
                entry.player->finishEating(transaction.itemRelease->hotbarSlot);
            } else {
                entry.player->releaseItem(transaction.itemRelease->hotbarSlot);
            }
            return;
        }
        if (!transaction.entityUse.has_value()) {
            return;
        }
        if (transaction.entityUse->action == kAttack) {
            entry.player->attack(transaction.entityUse->runtimeEntityId);
        } else {
            entry.player->useOnEntity(transaction.entityUse->runtimeEntityId);
        }
        return;
    }
    if (packet == BedrockPacket::MobEquipment && entry.player) {
        const auto slot = wire::bedrock::readHeldSlot(body, entry.link->protocolVersion());
        if (slot.has_value()) {
            entry.player->setHeldSlot(*slot);
        }
        return;
    }
    if (packet == BedrockPacket::Interact && entry.player) {
        constexpr std::int32_t kOpenInventory = 6;

        const auto interact = wire::bedrock::readInteract(body);
        if (interact.has_value() && interact->action == kOpenInventory) {
            entry.player->openOwnInventory();
        }
        return;
    }
    if (packet == BedrockPacket::ContainerClose && entry.player) {
        const auto close = wire::bedrock::readContainerClose(body);
        if (close.has_value()) {
            entry.player->closeWindow(close->containerId);
        }
        return;
    }
    if (packet == BedrockPacket::CommandRequest && entry.player) {
        const auto line = wire::bedrock::readCommandRequest(body);
        if (line.has_value()) {
            entry.player->command(*line);
        }
        return;
    }
    if (packet == BedrockPacket::ModalFormResponse && entry.player) {
        const auto answer = wire::bedrock::readModalFormResponse(body);
        if (answer.has_value()) {
            entry.player->formAnswer(answer->formId, answer->data);
        }
        return;
    }
    if (packet == BedrockPacket::PlayerAction && entry.player) {
        const auto action = wire::bedrock::readPlayerAction(body);
        if (action == wire::bedrock::BedrockPlayerAction::StopSleep) {
            entry.player->leaveBed();
        } else if (action.has_value()) {
            entry.player->settled(*action == wire::bedrock::BedrockPlayerAction::Respawn);
        }
        return;
    }
    if (packet == BedrockPacket::Respawn && entry.player) {
        const auto state = wire::bedrock::readRespawn(body);
        if (state == wire::bedrock::RespawnState::ClientReady) {
            entry.player->respawn();
        }
        return;
    }
    if (packet == BedrockPacket::LevelSoundEvent && entry.player) {
        // That edition asks for a sound its client already decided on; it goes only to whoever asked.
        entry.mailbox->post(static_cast<std::uint16_t>(packet),
                            std::vector<std::uint8_t>{body.readable().begin(), body.readable().end()});
        return;
    }
    if (packet == BedrockPacket::Animate && entry.player) {
        /// parity: `AnimatePacket.Action.SWING_ARM`, the only one of the seven a server acts on.
        constexpr std::int32_t kSwingArm = 1;

        const auto animation = wire::bedrock::readAnimate(body);
        if (animation.has_value() && animation->action == kSwingArm) {
            entry.player->swing();
        }
        return;
    }
    if (packet == BedrockPacket::BlockPickRequest && entry.player) {
        const auto pick = wire::bedrock::readBlockPickRequest(body);
        if (pick.has_value()) {
            entry.player->pickBlock(pick->x, pick->y, pick->z, pick->hotbarSlot);
        }
        return;
    }
    if (packet == BedrockPacket::EntityPickRequest && entry.player) {
        const auto pick = wire::bedrock::readEntityPickRequest(body);
        if (pick.has_value()) {
            entry.player->pickEntity(pick->runtimeEntityId);
        }
        return;
    }
    if (packet == BedrockPacket::ToggleCrafterSlotRequest && entry.player) {
        const auto toggle = wire::bedrock::readCrafterSlotToggle(body);
        if (toggle.has_value()) {
            entry.player->toggleCrafterSlot(toggle->x, toggle->y, toggle->z, toggle->slot, toggle->disabled);
        }
        return;
    }
    if (packet == BedrockPacket::Emote && entry.player) {
        const auto made = wire::bedrock::readEmote(body, entry.link->protocolVersion());
        if (made.has_value()) {
            entry.player->emote(made->emoteId, made->duration, made->flags);
        }
        return;
    }
    if (packet == BedrockPacket::CommandBlockUpdate && entry.player) {
        const auto update = wire::bedrock::readCommandBlockUpdate(body);
        if (update.has_value()) {
            entry.player->programCommandBlock(update->x, update->y, update->z, update->mode, update->command,
                                              update->trackOutput, update->conditional, update->needsRedstone);
        }
        return;
    }
    if (packet == BedrockPacket::LecternUpdate && entry.player) {
        const auto page = wire::bedrock::readLecternUpdate(body, entry.link->protocolVersion());
        if (page.has_value()) {
            entry.player->turnLecternPage(page->x, page->y, page->z, page->page);
        }
        return;
    }
    if (packet == BedrockPacket::BlockEntityData && entry.player) {
        const auto edit = wire::bedrock::readSignEdit(body);
        if (edit.has_value()) {
            entry.player->editSign(edit->x, edit->y, edit->z, edit->text);
        }
        return;
    }
    if (packet == BedrockPacket::PacketViolationWarning) {
        const std::optional<wire::bedrock::BedrockPacketViolation> violation =
            wire::bedrock::readPacketViolationWarning(body);
        if (violation.has_value()) {
            logger_.warn([&violation] {
                const std::string_view named =
                    wire::bedrock::bedrockPacketName(static_cast<std::uint16_t>(violation->packetId));
                return "A Bedrock client refused " +
                       (named.empty() ? "packet " + std::to_string(violation->packetId) : std::string{named}) +
                       " (severity " + std::to_string(violation->severity) + "): " + printable(violation->context);
            });
        }
        return;
    }
    if (packet == BedrockPacket::ShowCredits && entry.player) {
        const std::optional<wire::bedrock::BedrockCredits> credits = wire::bedrock::readShowCredits(body);
        if (credits.has_value() && *credits == wire::bedrock::BedrockCredits::Finished) {
            entry.player->respawn();
        }
        return;
    }
    if (packet == BedrockPacket::EntityEvent && entry.player) {
        const std::optional<wire::bedrock::BedrockEntityEventIn> event = wire::bedrock::readEntityEvent(body);
        if (event.has_value() &&
            event->event == static_cast<std::uint8_t>(wire::bedrock::BedrockEntityEvent::EatingItem)) {
            entry.mailbox->post(static_cast<std::uint16_t>(packet),
                                std::vector<std::uint8_t>{body.readable().begin(), body.readable().end()});
        }
        return;
    }
    if (packet == BedrockPacket::BookEdit && entry.player) {
        const std::optional<wire::bedrock::BedrockBookEdit> edit = wire::bedrock::readBookEdit(body);
        if (!edit.has_value()) {
            return;
        }
        using Action = wire::bedrock::BedrockBookEdit::Action;
        if (edit->action == Action::Sign) {
            entry.player->signBook(edit->slot, edit->title);
            return;
        }
        const world::books::PageEdit what = edit->action == Action::ReplacePage  ? world::books::PageEdit::Replace
                                            : edit->action == Action::AddPage    ? world::books::PageEdit::Add
                                            : edit->action == Action::DeletePage ? world::books::PageEdit::Delete
                                                                                 : world::books::PageEdit::Swap;
        entry.player->editBookPage(edit->slot, what, edit->page, edit->otherPage, edit->text);
        return;
    }
    if (packet == BedrockPacket::MapInfoRequest && entry.player) {
        if (const std::optional<std::int64_t> map = wire::bedrock::readMapInfoRequest(body); map.has_value()) {
            entry.player->requestMap(*map);
        }
        return;
    }
    if (packet == BedrockPacket::FilterText && entry.player) {
        const std::optional<std::string> text = wire::bedrock::readFilterText(body);
        if (text.has_value()) {
            wire::io::ByteBuffer out;
            wire::bedrock::writeFilterText(out, *text);
            entry.mailbox->post(static_cast<std::uint16_t>(BedrockPacket::FilterText),
                                std::vector<std::uint8_t>{out.readable().begin(), out.readable().end()});
        }
        return;
    }
    if (packet == BedrockPacket::Text && entry.player) {
        const auto text = wire::bedrock::readText(body);
        if (text.has_value() && !text->message.empty()) {
            entry.player->chat(text->message);
        }
        return;
    }
    if (entry.unanswered.insert(static_cast<std::uint16_t>(packet)).second) {
        logger_.debug([packet] {
            return "Nothing answers Bedrock " +
                   std::string{wire::bedrock::bedrockPacketName(static_cast<std::uint16_t>(packet))} + " (" +
                   std::to_string(static_cast<std::int32_t>(packet)) + ").";
        });
    }
}

void BedrockListener::onMessage(RakSession& session, std::span<const std::uint8_t> payload) {
    const auto found = sessions_.find(session.peer());
    if (found == sessions_.end()) {
        return;
    }
    found->second->rak->receive(payload);
}

void BedrockListener::onDisconnect(RakSession& session) {
    // The counts tell a client that disliked a packet from one that went quiet; a stalled burst names the transport.
    logger_.debug([&session] {
        const wire::bedrock::RakSession::Stats& stats = session.stats();
        return std::string{"Bedrock session ended: the client "} +
               (session.saidGoodbye() ? "said goodbye" : "went quiet") + " after " +
               std::to_string(stats.datagramsSent) + " datagram(s) out (" +
               std::to_string(stats.datagramsAcknowledged) + " acknowledged, " +
               std::to_string(stats.datagramsAskedAgain) + " asked for again, " +
               std::to_string(stats.datagramsResent) + " resent) and " + std::to_string(stats.datagramsReceived) +
               " in, with " + std::to_string(stats.splitMessages) + " message(s) split into " +
               std::to_string(stats.splitParts) + " part(s).";
    });
    if (const auto found = sessions_.find(session.peer()); found != sessions_.end()) {
        skins_->forget(
            uuidOf(found->second->login.xuid.empty() ? found->second->login.name : found->second->login.xuid));
    }
    sessions_.erase(session.peer());
}

void BedrockListener::onReady(Session& session) {
    if (session.started) {
        return;
    }
    session.started = true;

    // TEMPORARY: `BASALT_BEDROCK_DROP=items` leaves one packet out of the burst to find the one a client refuses.
    const char* const dropped = std::getenv("BASALT_BEDROCK_DROP");
    const std::string drop = dropped == nullptr ? std::string{} : std::string{dropped};
    const auto dropping = [&drop](const std::string_view what) { return drop.find(what) != std::string::npos; };
    if (!drop.empty()) {
        logger_.warn([&drop] { return "Leaving out of the Bedrock join: " + drop; });
    }

    // Piece sizes tell whether a client that never answers chokes on one packet's contents or on the size.
    const auto note = [this](const std::string_view what, const std::size_t bytes) {
        logger_.debug([what, bytes] {
            return "Bedrock join: " + std::string{what} + " is " + std::to_string(bytes) + " byte(s).";
        });
    };

    const wire::common::Uuid uuid = uuidOf(session.login.xuid.empty() ? session.login.name : session.login.xuid);
    const Spawn spawn = hooks_.spawn ? hooks_.spawn(uuid) : Spawn{};

    wire::bedrock::StartGameInfo info;
    info.x = static_cast<float>(spawn.x);
    info.y = static_cast<float>(spawn.y);
    info.z = static_cast<float>(spawn.z);
    info.yaw = spawn.yaw;
    info.pitch = spawn.pitch;
    info.playerGameType = spawn.gamemode;
    // The client is told where its eyes are, the same offset it reports back on every movement.
    info.y += wire::bedrock::kPlayerEyeHeight;
    info.vanillaVersion = gameVersion_;
    info.serverId = "basalt";
    info.worldId = "basalt";

    wire::io::ByteBuffer body;
    wire::bedrock::writeStartGame(body, info, session.link->protocolVersion());
    session.link->sendPacket(BedrockPacket::StartGame, body.readable());
    note("start_game", body.readableBytes());

    // A real client reads start_game, finds it knows nothing of what lives here, and leaves without a word.
    body.clear();
    if (!dropping("biomes")) {
        wire::bedrock::writeBiomeDefinitionList(body);
        session.link->sendPacket(BedrockPacket::BiomeDefinitionList, body.readable());
        note("biomes", body.readableBytes());
    }

    // Item numbers are said by the server and go before anything that could carry an item.
    body.clear();
    if (!dropping("items")) {
        wire::bedrock::writeItemComponent(body, session.link->protocolVersion());
        session.link->sendPacket(BedrockPacket::ItemComponent, body.readable());
        note("items", body.readableBytes());
    }

    body.clear();
    if (!dropping("entities")) {
        wire::bedrock::writeAvailableEntityIdentifiers(body);
        session.link->sendPacket(BedrockPacket::AvailableEntityIdentifiers, body.readable());
        note("entities", body.readableBytes());
    }

    body.clear();
    if (!dropping("creative")) {
        // TEMPORARY: `BASALT_BEDROCK_CREATIVE=n` offers only the first n items, to find the entry a client refuses.
        const char* const most = std::getenv("BASALT_BEDROCK_CREATIVE");
        const std::size_t offered = most == nullptr ? SIZE_MAX : static_cast<std::size_t>(std::atoll(most));
        wire::bedrock::writeCreativeContent(body, session.link->protocolVersion(), offered);
        session.link->sendPacket(BedrockPacket::CreativeContent, body.readable());
        note("creative", body.readableBytes());
    }

    body.clear();
    wire::bedrock::writeGameRulesChanged(body, session.link->protocolVersion());
    session.link->sendPacket(BedrockPacket::GameRulesChanged, body.readable());
    note("gamerules", body.readableBytes());

    // A value for a property the client was never declared draws nothing, so each kind that has one is named once.
    for (const std::string_view farmAnimal : {"minecraft:pig", "minecraft:cow", "minecraft:chicken"}) {
        static constexpr std::array<std::string_view, 3> kClimates{"temperate", "warm", "cold"};
        body.clear();
        wire::bedrock::writeSyncEntityProperty(body, farmAnimal, "minecraft:climate_variant", kClimates);
        session.link->sendPacket(BedrockPacket::SyncEntityProperty, body.readable());
    }
    note("entity properties", body.readableBytes());

    // An instruction pointing at a preset the client was not told about is dropped, so presets are named once.
    body.clear();
    wire::bedrock::writeCameraPresets(body, session.link->protocolVersion());
    session.link->sendPacket(BedrockPacket::CameraPresets, body.readable());
    note("camera presets", body.readableBytes());

    // A grid shows no result unless the recipe behind it came from here, so the client's recipe book is ours.
    body.clear();
    wire::bedrock::writeCraftingData(body,
                                     hooks_.recipes ? hooks_.recipes() : std::vector<wire::bedrock::BedrockRecipe>{},
                                     session.link->protocolVersion());
    session.link->sendPacket(BedrockPacket::CraftingData, body.readable());
    note("recipes", body.readableBytes());

    body.clear();
    if (!dropping("commands")) {
        wire::bedrock::writeSetCommandsEnabled(body, true);
        session.link->sendPacket(BedrockPacket::SetCommandsEnabled, body.readable());
        note("commands", body.readableBytes());
    }

    body.clear();
    wire::bedrock::writeChunkRadiusUpdated(body, kInitialRadius);
    session.link->sendPacket(BedrockPacket::ChunkRadiusUpdated, body.readable());

    body.clear();
    wire::bedrock::writeNetworkChunkPublisherUpdate(body, static_cast<std::int32_t>(info.x),
                                                    static_cast<std::int32_t>(info.y),
                                                    static_cast<std::int32_t>(info.z), kInitialRadius * 16);
    session.link->sendPacket(BedrockPacket::NetworkChunkPublisherUpdate, body.readable());

    // No empty columns: sending a hole first and the ground a moment later is worse than the ground a moment later.

    session.link->flush();

    if (!hooks_.join) {
        return;
    }
    Joining joining;
    joining.gamertag = session.login.name;
    joining.name = prefixedName(session.login.name);
    joining.xuid = session.login.xuid;
    joining.uuid = uuid;
    joining.address = session.address;
    joining.authenticated = session.login.authenticated;
    joining.protocol = session.link->protocolVersion();
    joining.mailbox = session.mailbox;
    joining.skins = skins_;
    if (!session.login.skinPixels.empty()) {
        wire::bedrock::SkinInfo skin;
        skin.skinId = session.login.skinId.empty() ? joining.name : session.login.skinId;
        if (!session.login.skinGeometry.empty()) {
            // Keeping the default is what says: the geometry you already have.
            skin.resourcePatch = session.login.skinGeometry;
        }
        skin.width = session.login.skinWidth;
        skin.height = session.login.skinHeight;
        skin.image = session.login.skinPixels;
        skins_->remember(joining.uuid, skin);
    }
    session.player = hooks_.join(joining);
}

void BedrockListener::deliverMail(Session& session) {
    const std::vector<BedrockMailbox::Item> items = session.mailbox->drain(kTurnBytes);
    if (items.empty()) {
        return;
    }
    for (const BedrockMailbox::Item& item : items) {
        if (item.body.size() >= kLargePacketBytes) {
            logger_.debug([&item] {
                return "Bedrock out: " + std::string{wire::bedrock::bedrockPacketName(item.packetId)} + " is " +
                       std::to_string(item.body.size()) + " byte(s).";
            });
        }
        session.link->sendPacket(static_cast<BedrockPacket>(item.packetId), item.body);
        if (item.last) {
            // The goodbye was written by whoever posted it; the client leaves on its own and RakNet clears the session.
            session.link->flush();
            session.link->close();
            return;
        }
    }
    session.link->flush();
}

} // namespace basalt::server::runtime::bedrock
