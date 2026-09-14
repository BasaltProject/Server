#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <chrono>
#include <cstdlib>
#include <set>
#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

/// What one line of a stack request is called, for the log line that says what a client asked for.
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
    case wire::bedrock::BedrockStackActionType::Other:
        break;
    }
    return "one this build does not act on";
}

using wire::bedrock::BedrockConnection;
using wire::bedrock::BedrockPacket;
using wire::bedrock::RakSession;

/// How long a poll waits for a datagram before going round again. Twenty milliseconds is two ticks
/// of nothing, which is short enough that an acknowledgement is never late and long enough that an
/// idle server is not a spinning thread.
constexpr int kPollTimeoutMillis = 20;

/// What a client is told to draw before it has asked for anything.
constexpr std::int32_t kInitialRadius = 4;

/// How much one turn may hand to a client. A join used to glue every column into a single batch of
/// well over a megabyte: one RakNet message split into a thousand parts, none of them worth
/// anything until the last one arrived. A turn's worth at a time is the same bytes in pieces a
/// client can read as they land.
constexpr std::size_t kTurnBytes = 64U * 1024U;

/// Above this a packet is named in the log on its way out. A join is made of a handful of these and
/// a stream of small ones, so naming only the big ones says what a slow join was busy with, and
/// leaves the last one in the log when a client reads something it cannot.
constexpr std::size_t kLargePacketBytes = 1024;

[[nodiscard]] std::uint64_t nowMillis() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

/// A semicolon separates every field of the line a Bedrock client reads, so one inside the message
/// would shift everything after it. A newline would do worse.
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

/// A Bedrock server list shows two lines, and a Java message of the day is written as two. The
/// first is the name, the second is whatever the operator put under it.
[[nodiscard]] std::pair<std::string, std::string> splitMotd(std::string_view text) {
    const std::size_t newline = text.find('\n');
    if (newline == std::string_view::npos) {
        return {oneLine(text), std::string{}};
    }
    return {oneLine(text.substr(0, newline)), oneLine(text.substr(newline + 1))};
}

} // namespace

/// One client: its transport, its protocol state, the player the server made of it, and the
/// mailbox anyone else writes to.
struct BedrockListener::Session {
    RakSession& transport;
    BedrockConnection connection;
    std::shared_ptr<BedrockMailbox> mailbox = std::make_shared<BedrockMailbox>();
    std::unique_ptr<BedrockListener::Player> player;
    wire::bedrock::LoginRequest login;
    bool started = false;
    /// TEMPORARY: said once per client, not once per packet.
    bool saidBadInput = false;
    /// Which kinds of packet this server has already been seen to ignore, so the log names each one
    /// once rather than every tick.
    std::set<std::uint16_t> unanswered;

    Session(RakSession& session, std::shared_ptr<const wire::bedrock::EcKeyPair> keys, std::string gameVersion,
            wire::bedrock::BedrockGate gate)
        : transport(session), connection(session, std::move(keys), std::move(gameVersion), std::move(gate)) {}
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
        // Ahead of everything still waiting, and the drain is bounded: a goodbye queued behind a
        // world's worth of columns would take several turns to reach, and there are none left.
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
    // The same shape as the offline uuid vanilla derives from a name, with a prefix of its own so
    // the two spaces cannot meet.
    return wire::common::Uuid::nameBased("BedrockPlayer:" + std::string{xuid});
}

std::string BedrockListener::advertisement() const {
    const Status status = hooks_.status ? hooks_.status() : Status{"Basalt", 0, 20};
    const auto [motd, subMotd] = splitMotd(status.motd);

    // The order is the client's, not ours: edition, the line, the protocol, the version, who is on,
    // how many fit, the server's own id, the second line, the game mode twice, and the two ports.
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
    // One last turn of the post, now that nobody else can touch a session: whoever was told the
    // server is closing was told it after the thread had stopped, and their goodbye is still here.
    for (const auto& [peer, session] : sessions_) {
        deliverMail(*session);
    }
    // And one turn of the socket to put it on the wire. Everything above only queues: the bytes of
    // a session reach the network inside the poll, and the thread that was doing the polling is the
    // one that just stopped.
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
        // Before the poll rather than after it: what the poll flushes is what is queued now, so a
        // line left by another thread goes out on this turn instead of the next.
        const bool open = accepting_.load();
        for (const auto& [peer, session] : sessions_) {
            // Once the door is closed the server is saving these same players: the post keeps
            // running so their goodbye reaches them, but nothing here touches them any more.
            if (session->player != nullptr && open) {
                session->player->tick();
            }
            deliverMail(*session);
        }
        server_->poll(nowMillis(), kPollTimeoutMillis);
    }
}

void BedrockListener::onConnect(RakSession& session) {
    // Shutting down. Letting one in here means a player who joins mid-save and is never saved.
    if (!accepting_.load()) {
        return;
    }
    auto owned = std::make_unique<Session>(session, keys_, gameVersion_, options_.gate);
    Session& entry = *owned;
    sessions_.emplace(session.peer(), std::move(owned));

    entry.connection.onLogin([this, &entry](const wire::bedrock::LoginRequest& login) {
        entry.login = login;
        logger_.info([this, &login] {
            return prefixedName(login.name) + " is joining from Bedrock" +
                   (login.authenticated ? "" : " (unauthenticated)") + ".";
        });
    });
    entry.connection.onRefused([this](const int protocol, const std::string& told) {
        logger_.info([protocol, &told] {
            return "A Bedrock client on protocol " + std::to_string(protocol) + " was turned away: " + told;
        });
    });
    entry.connection.onWarning([this](const int protocol) {
        logger_.warn([protocol] {
            return "A Bedrock client on protocol " + std::to_string(protocol) +
                   " is newer than this server speaks and was let in anyway; it may not get far.";
        });
    });
    entry.connection.onReady([this, &entry] { onReady(entry); });
    entry.connection.onPacket([this, &entry](BedrockPacket packet, wire::io::ByteBuffer& body) {
        if (packet == BedrockPacket::RequestChunkRadius) {
            // The client asks for a draw distance of its own once it is in.
            const auto radius = wire::bedrock::readRequestChunkRadius(body);
            if (!radius) {
                return;
            }
            wire::io::ByteBuffer answer;
            wire::bedrock::writeChunkRadiusUpdated(answer, *radius);
            entry.connection.sendPacket(BedrockPacket::ChunkRadiusUpdated, answer.readable());
            entry.connection.flush();
            return;
        }
        if (packet == BedrockPacket::PlayerAuthInput && entry.player) {
            const auto input = wire::bedrock::readPlayerAuthInput(body, entry.connection.protocolVersion());
            // TEMPORARY: an input this server cannot read is a player who never moves, never digs
            // and never places anything, and nothing else in the log says so.
            if (!input.has_value() && !entry.saidBadInput) {
                entry.saidBadInput = true;
                logger_.warn([&entry] {
                    return "Could not read player_auth_input from a client on protocol " +
                           std::to_string(entry.connection.protocolVersion()) + ".";
                });
            }
            if (input.has_value()) {
                // Before the click, because it is what decides what the click means.
                entry.player->setSneaking(input->sneaking);
                entry.player->setStance(input->onGround, input->sprinting);
                // Zero is a click on a block; one is a click on the air and two is breaking, and
                // neither of those puts anything down.
                // TEMPORARY: what the client says it did with what it holds, which is the only way
                // to tell a placement this server refuses from one it never heard.
                if (input->itemUse.has_value()) {
                    logger_.debug([&input] {
                        return "item use " + std::to_string(input->itemUse->action) + " at " +
                               std::to_string(input->itemUse->x) + ", " + std::to_string(input->itemUse->y) + ", " +
                               std::to_string(input->itemUse->z) + " face " + std::to_string(input->itemUse->face) +
                               " slot " + std::to_string(input->itemUse->hotbarSlot);
                    });
                }
                if (input->itemUse.has_value() && input->itemUse->action == 0) {
                    entry.player->useItemOn(input->itemUse->x, input->itemUse->y, input->itemUse->z,
                                            input->itemUse->face, input->itemUse->clickX, input->itemUse->clickY,
                                            input->itemUse->clickZ, input->itemUse->hotbarSlot);
                }
                if (input->stackRequest.has_value()) {
                    // What the client asked for, by line, before anything acts on it. A request
                    // this build refuses says why here rather than only failing: the lines that
                    // craft are the ones this server cannot answer yet.
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
                    // TEMPORARY: what the client actually asks for when somebody digs, which is the
                    // only way to tell a break this server refuses from one it never heard.
                    logger_.debug([&action] {
                        return "block action " + std::to_string(action.action) + " at " + std::to_string(action.x) +
                               ", " + std::to_string(action.y) + ", " + std::to_string(action.z);
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
            /// Zero interacts with the entity, one hits it.
            constexpr std::int32_t kAttack = 1;

            const auto use = wire::bedrock::readEntityUse(body, entry.connection.protocolVersion());
            if (!use.has_value()) {
                return;
            }
            if (use->action == kAttack) {
                entry.player->attack(use->runtimeEntityId);
            } else {
                // Everything that is not a hit: sitting in a cart, feeding an animal, shearing.
                entry.player->useOnEntity(use->runtimeEntityId);
            }
            return;
        }
        if (packet == BedrockPacket::MobEquipment && entry.player) {
            const auto slot = wire::bedrock::readHeldSlot(body, entry.connection.protocolVersion());
            if (slot.has_value()) {
                entry.player->setHeldSlot(*slot);
            }
            return;
        }
        if (packet == BedrockPacket::Interact && entry.player) {
            /// What the client calls asking for its own bag. The others are mouse-overs and
            /// vehicles, which this server settles without being told.
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
        if (packet == BedrockPacket::Respawn && entry.player) {
            const auto state = wire::bedrock::readRespawn(body);
            if (state == wire::bedrock::RespawnState::ClientReady) {
                entry.player->respawn();
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
        // Everything this server has no answer for, named once each. A client asks for far more
        // than it is told, and which of those asks go unanswered is not something the reference
        // implementation's own list can say: only a real client can.
        if (entry.unanswered.insert(static_cast<std::uint16_t>(packet)).second) {
            logger_.debug([packet] {
                return "Nothing answers Bedrock " +
                       std::string{wire::bedrock::bedrockPacketName(static_cast<std::uint16_t>(packet))} + " (" +
                       std::to_string(static_cast<std::int32_t>(packet)) + ").";
            });
        }
    });
}

void BedrockListener::onMessage(RakSession& session, std::span<const std::uint8_t> payload) {
    const auto found = sessions_.find(session.peer());
    if (found == sessions_.end()) {
        return;
    }
    found->second->connection.receive(payload);
}

void BedrockListener::onDisconnect(RakSession& session) {
    // A client that said goodbye read something it did not like; one that simply went quiet was not
    // being reached. The counts say which, and a burst that stops being acknowledged halfway names
    // the transport rather than the packet.
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
        // Their skin leaves with them: nobody else needs to draw somebody who is not here.
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

    // TEMPORARY, to find the one a real client refuses: `BASALT_BEDROCK_DROP=items` leaves that one
    // out and sends the other four. One missing is a burst a client can still make sense of; five
    // missing is not a test, which is what the last round of this measured by mistake.
    const char* const dropped = std::getenv("BASALT_BEDROCK_DROP");
    const std::string drop = dropped == nullptr ? std::string{} : std::string{dropped};
    const auto dropping = [&drop](const std::string_view what) { return drop.find(what) != std::string::npos; };
    if (!drop.empty()) {
        logger_.warn([&drop] { return "Leaving out of the Bedrock join: " + drop; });
    }

    // How big each piece of the burst is, which is what says whether a client that never answers
    // is choking on one packet's contents or on the size of it.
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
    // The client is told where its eyes are, not its feet, which is the same offset it reports
    // back on every movement.
    info.y += wire::bedrock::kPlayerEyeHeight;
    info.vanillaVersion = gameVersion_;
    info.serverId = "basalt";
    info.worldId = "basalt";

    wire::io::ByteBuffer body;
    wire::bedrock::writeStartGame(body, info, session.connection.protocolVersion());
    session.connection.sendPacket(BedrockPacket::StartGame, body.readable());
    note("start_game", body.readableBytes());

    // What a client is told about the world before it is told it is in it. A real client reads
    // start_game, finds it knows nothing about what lives here, and leaves without a word.
    body.clear();
    if (!dropping("biomes")) {
        wire::bedrock::writeBiomeDefinitionList(body);
        session.connection.sendPacket(BedrockPacket::BiomeDefinitionList, body.readable());
        note("biomes", body.readableBytes());
    }

    // Which item is which number, said by the server rather than agreed by version. It goes before
    // anything that could carry an item, which is everything after this point.
    body.clear();
    if (!dropping("items")) {
        wire::bedrock::writeItemComponent(body);
        session.connection.sendPacket(BedrockPacket::ItemComponent, body.readable());
        note("items", body.readableBytes());
    }

    body.clear();
    if (!dropping("entities")) {
        wire::bedrock::writeAvailableEntityIdentifiers(body);
        session.connection.sendPacket(BedrockPacket::AvailableEntityIdentifiers, body.readable());
        note("entities", body.readableBytes());
    }

    body.clear();
    if (!dropping("creative")) {
        // TEMPORARY: `BASALT_BEDROCK_CREATIVE=n` offers only the first n items, which is how to ask
        // a client which entry it refuses when the packet is otherwise well formed.
        const char* const most = std::getenv("BASALT_BEDROCK_CREATIVE");
        const std::size_t offered = most == nullptr ? SIZE_MAX : static_cast<std::size_t>(std::atoll(most));
        wire::bedrock::writeCreativeContent(body, session.connection.protocolVersion(), offered);
        session.connection.sendPacket(BedrockPacket::CreativeContent, body.readable());
        note("creative", body.readableBytes());
    }

    // The handful of rules that stop the client acting where this server already does.
    body.clear();
    wire::bedrock::writeGameRulesChanged(body, session.connection.protocolVersion());
    session.connection.sendPacket(BedrockPacket::GameRulesChanged, body.readable());
    note("gamerules", body.readableBytes());

    // The recipe book, empty for now. The inventory screen is built around it, and a client that
    // was never sent one has nothing to open.
    body.clear();
    wire::bedrock::writeCraftingData(body);
    session.connection.sendPacket(BedrockPacket::CraftingData, body.readable());
    note("recipes", body.readableBytes());

    body.clear();
    if (!dropping("commands")) {
        wire::bedrock::writeSetCommandsEnabled(body, true);
        session.connection.sendPacket(BedrockPacket::SetCommandsEnabled, body.readable());
        note("commands", body.readableBytes());
    }

    body.clear();
    wire::bedrock::writeChunkRadiusUpdated(body, kInitialRadius);
    session.connection.sendPacket(BedrockPacket::ChunkRadiusUpdated, body.readable());

    body.clear();
    wire::bedrock::writeNetworkChunkPublisherUpdate(body, static_cast<std::int32_t>(info.x),
                                                    static_cast<std::int32_t>(info.y),
                                                    static_cast<std::int32_t>(info.z), kInitialRadius * 16);
    session.connection.sendPacket(BedrockPacket::NetworkChunkPublisherUpdate, body.readable());

    // No empty columns here any more: the player made just below asks the world for the real
    // ones, which answer the same question and answer it with blocks in them. Sending a hole
    // first and the ground a moment later is worse than sending the ground a moment later.

    session.connection.flush();

    if (!hooks_.join) {
        return;
    }
    Joining joining;
    joining.gamertag = session.login.name;
    joining.name = prefixedName(session.login.name);
    joining.xuid = session.login.xuid;
    joining.uuid = uuid;
    joining.address = session.transport.peer().toString();
    joining.authenticated = session.login.authenticated;
    joining.protocol = session.connection.protocolVersion();
    joining.mailbox = session.mailbox;
    joining.skins = skins_;
    // What the client said it looks like, kept for everybody else to draw. One that sent nothing
    // readable is remembered as nothing, and whoever draws it falls back to a flat colour.
    if (!session.login.skinPixels.empty()) {
        wire::bedrock::SkinInfo skin;
        skin.skinId = session.login.skinId.empty() ? joining.name : session.login.skinId;
        if (!session.login.skinGeometry.empty()) {
            // Keeping the default here is what says "the geometry you already have".
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
        session.connection.sendPacket(static_cast<BedrockPacket>(item.packetId), item.body);
        if (item.last) {
            // The goodbye was written by whoever posted it, so this only has to stop: the client
            // leaves on its own once it has read it, and RakNet clears the session up after.
            session.connection.flush();
            session.connection.close();
            return;
        }
    }
    session.connection.flush();
}

} // namespace basalt::server::runtime::bedrock
