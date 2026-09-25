#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaTagData.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/login/LoginPluginPackets.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace login = wire::protocol::login;
namespace mv = wire::multiversion;
namespace reg = server::registry;

using wire::common::Identifier;
using wire::common::Uuid;

/// The line a refused client reads, whichever way its version is wrong.
std::string versionRefusalOf(mv::ProtocolVerdict::Refused::Reason reason) {
    const std::string version{buildInfo::kMinecraftVersion};
    if (reason == mv::ProtocolVerdict::Refused::Reason::TooOld) {
        return "<red>Your version is too old for this server.</red>\n<gray>It runs Minecraft " + version + ".";
    }
    if (reason == mv::ProtocolVerdict::Refused::Reason::Snapshot) {
        return "<red>This server does not accept snapshots.</red>\n<gray>It runs Minecraft " + version + ".";
    }
    return "<red>This server does not accept your version.</red>\n<gray>It runs Minecraft " + version + ".";
}

std::string_view reasonName(mv::ProtocolVerdict::Refused::Reason reason) {
    switch (reason) {
    case mv::ProtocolVerdict::Refused::Reason::TooOld:
        return "too_old";
    case mv::ProtocolVerdict::Refused::Reason::TooNew:
        return "too_new";
    case mv::ProtocolVerdict::Refused::Reason::Snapshot:
        return "snapshot";
    case mv::ProtocolVerdict::Refused::Reason::NotTranslated:
        return "not_translated";
    }
    return "too_old";
}

/// The seven registries in `RegistryDataLoader.SYNCHRONIZED_REGISTRIES` that nothing else here
/// populates yet. Sent with zero entries so the client's registry access has an entry for each,
/// which the tags scoped to them need to resolve at all.
constexpr std::array<std::string_view, 7> kEmptySynchronizedRegistries{
    "minecraft:trim_pattern",     "minecraft:sulfur_cube_archetype", "minecraft:enchantment",
    "minecraft:test_environment", "minecraft:test_instance",         "minecraft:dialog",
    "minecraft:timeline"};

} // namespace

void ClientSession::handleHandshake(std::int32_t packetId, wire::io::ByteBuffer& body) {
    if (packetId != wire::protocol::handshake::HandshakePacket::kId) {
        return;
    }
    const std::optional<wire::protocol::handshake::HandshakePacket> packet =
        wire::protocol::handshake::HandshakePacket::read(
            body, proxy_.readsHandshake() ? wire::protocol::handshake::HandshakePacket::kForwardedHostLength
                                          : wire::protocol::handshake::HandshakePacket::kMaxHostLength);
    if (!packet.has_value()) {
        connection_.close();
        return;
    }
    handshake_ = packet;
    verdict_ = options_.protocolGate.verdictFor(packet->protocolVersion);
    translation_ = mv::java::javaTranslators::chainFor(packet->protocolVersion, options_.substitutionLabels);
    switch (packet->intent) {
    case wire::protocol::handshake::ClientIntent::Status:
        state_ = wire::protocol::ProtocolState::Status;
        break;
    case wire::protocol::handshake::ClientIntent::Login:
        state_ = wire::protocol::ProtocolState::Login;
        break;
    case wire::protocol::handshake::ClientIntent::Transfer:
        // Not supported yet: the connection is dropped rather than half-answered.
        connection_.close();
        return;
    }
    // Read now, acted on at login: a refusal there is still a disconnect screen rather than a socket
    // that closes for no stated reason. A ping never carries forwarded data.
    if (proxy_.readsHandshake() && packet->intent == wire::protocol::handshake::ClientIntent::Login) {
        forwardedHandshake_ = proxy_.readHandshake(packet->hostName);
        if (const auto* accepted = std::get_if<wire::auth::forwarding::ForwardedLogin>(&*forwardedHandshake_);
            accepted != nullptr) {
            // The host with the forwarded data stripped back off it, which is what the player typed.
            handshake_->hostName = accepted->host;
        }
    }
}

void ClientSession::handleStatus(std::int32_t packetId, wire::io::ByteBuffer& body) {
    if (packetId == wire::protocol::status::kStatusRequestId) {
        const std::int32_t protocol = handshake_.has_value() ? handshake_->protocolVersion : 0;
        if (!options_.statusProvider) {
            return;
        }
        wire::protocol::status::ServerStatusPayload status = options_.statusProvider(
            protocol, handshake_.has_value() ? handshake_->hostName : "", connection_.remoteAddress());
        // A client compares this number with its own and paints the entry red when they differ, so
        // echoing an accepted client's own number is what stops a server that would let them play
        // from looking broken in their list. Refusing one keeps our number, which is the honest hint.
        const bool refused = std::holds_alternative<mv::ProtocolVerdict::Refused>(verdict_.value);
        if (status.version.has_value() && status.version->protocol != protocol && !refused) {
            status.version->protocol = protocol;
        }
        sendUncompressed(wire::protocol::status::StatusResponsePacket::kId, [&status](wire::io::ByteBuffer& buffer) {
            wire::protocol::status::StatusResponsePacket{.status = status}.write(buffer);
        });
        return;
    }
    if (packetId == wire::protocol::status::PingPacket::kId) {
        const std::optional<wire::protocol::status::PingPacket> ping = wire::protocol::status::PingPacket::read(body);
        if (!ping.has_value()) {
            return;
        }
        sendUncompressed(wire::protocol::status::PingPacket::kId, [&ping](wire::io::ByteBuffer& buffer) {
            wire::protocol::status::PingPacket{.time = ping->time}.write(buffer);
        });
    }
}

void ClientSession::handleLogin(std::int32_t packetId, wire::io::ByteBuffer& body) {
    if (packetId == login::LoginStartPacket::kId) {
        handleLoginStart(body);
        return;
    }
    if (packetId == login::KeyPacket::kId) {
        handleKey(body);
        return;
    }
    if (packetId == login::LoginPluginResponsePacket::kId) {
        handleForwardingAnswer(body);
        return;
    }
    if (packetId == login::kLoginAcknowledgedId) {
        enterConfiguration();
    }
}

void ClientSession::handleLoginStart(wire::io::ByteBuffer& body) {
    const std::optional<login::LoginStartPacket> loginStart = login::LoginStartPacket::read(body);
    if (!loginStart.has_value()) {
        connection_.close();
        return;
    }
    if (const auto* refusal = std::get_if<mv::ProtocolVerdict::Refused>(&verdict_.value); refusal != nullptr) {
        refuseVersion(loginStart->name, *refusal);
        return;
    }
    if (const auto* unknown = std::get_if<mv::ProtocolVerdict::Unknown>(&verdict_.value);
        unknown != nullptr && unknown->warn) {
        logger_.warn([&loginStart, unknown] {
            return loginStart->name + " is on " + mv::protocolVersions::describe(unknown->protocol) +
                   ", which this build does not know. Letting them in untranslated; anything may happen.";
        });
    }
    if (const auto* translated = std::get_if<mv::ProtocolVerdict::Translated>(&verdict_.value); translated != nullptr) {
        logger_.info([&loginStart, translated] {
            return loginStart->name + " is on " + translated->version.toString() + ", translated to " +
                   std::string{buildInfo::kMinecraftVersion} + ".";
        });
    }
    pendingUsername_ = loginStart->name;
    // A proxy has already authenticated this player, so the session exchange never happens: what it
    // says about them is the whole of their identity here.
    if (proxy_.enabled()) {
        startForwardedLogin(*loginStart);
        return;
    }
    if (pluginRefusesLogin(loginStart->name, loginStart->profileId.toString())) {
        return;
    }
    wire::auth::SessionService* online = options_.onlineMode;
    if (online == nullptr) {
        finishLogin(Uuid::offlinePlayer(loginStart->name), loginStart->name, {});
        return;
    }
    challenge_.resize(4);
    for (std::uint8_t& byte : challenge_) {
        byte = static_cast<std::uint8_t>((options_.nextSeed ? options_.nextSeed() : 0) & 0xFF);
    }
    const std::span<const std::uint8_t> publicKey = options_.keyPair->publicKeyDer();
    sendUncompressed(login::HelloPacket::kId, [this, publicKey](wire::io::ByteBuffer& buffer) {
        login::HelloPacket{.serverId = "",
                           .publicKey = std::vector<std::uint8_t>(publicKey.begin(), publicKey.end()),
                           .challenge = challenge_,
                           .shouldAuthenticate = true}
            .write(buffer);
    });
}

bool ClientSession::pluginRefusesLogin(const std::string& name, const std::string& profileId) {
    // The one gate before a player exists at all: a plugin that says no here means they never join,
    // and what it says is what they read on the disconnect screen.
    if (!options_.plugins.loginRefused) {
        return false;
    }
    const std::optional<std::string> reason =
        options_.plugins.loginRefused(name, profileId, remoteAddress().value_or(""));
    if (!reason.has_value()) {
        return false;
    }
    disconnectFromLogin(reason->empty() ? "<red>You may not join this server" : *reason);
    return true;
}

void ClientSession::startForwardedLogin(const login::LoginStartPacket& loginStart) {
    if (proxy_.asksTheProxy()) {
        const std::int32_t transactionId = proxy_.beginQuery();
        const std::vector<std::uint8_t> query = proxy_.query();
        const std::string channel{proxy_.channel()};
        sendUncompressed(login::LoginPluginRequestPacket::kId, [transactionId, &channel,
                                                                &query](wire::io::ByteBuffer& buffer) {
            login::LoginPluginRequestPacket{.transactionId = transactionId, .channel = channel, .payload = query}.write(
                buffer);
        });
        return;
    }
    const wire::auth::forwarding::ForwardingResult result =
        forwardedHandshake_.value_or(wire::auth::forwarding::ForwardingResult{
            std::string{"the handshake was read before forwarding was configured"}});
    const std::optional<std::string> refusal = proxy_.accept(result, loginStart.name, connection_.remoteAddress());
    if (refusal.has_value()) {
        disconnectFromLogin(*refusal);
        return;
    }
    if (!proxy_.login().has_value()) {
        return;
    }
    const wire::auth::forwarding::ForwardedLogin& forwarded = *proxy_.login();
    if (pluginRefusesLogin(loginStart.name, forwarded.profileId.toString())) {
        return;
    }
    finishLogin(forwarded.profileId, loginStart.name, forwarded.properties);
}

void ClientSession::handleForwardingAnswer(wire::io::ByteBuffer& body) {
    const std::optional<login::LoginPluginResponsePacket> answer = login::LoginPluginResponsePacket::read(body);
    if (!answer.has_value() || !proxy_.asksTheProxy() || !pendingUsername_.has_value()) {
        connection_.close();
        return;
    }
    const wire::auth::forwarding::ForwardingResult result = proxy_.readAnswer(answer->transactionId, answer->payload);
    const std::optional<std::string> refusal = proxy_.accept(result, *pendingUsername_, connection_.remoteAddress());
    if (refusal.has_value()) {
        disconnectFromLogin(*refusal);
        return;
    }
    if (!proxy_.login().has_value()) {
        return;
    }
    const wire::auth::forwarding::ForwardedLogin& forwarded = *proxy_.login();
    // The proxy's name for them, not the one they claimed in the login packet.
    const std::string name = forwarded.name.value_or(*pendingUsername_);
    if (pluginRefusesLogin(name, forwarded.profileId.toString())) {
        return;
    }
    finishLogin(forwarded.profileId, name, forwarded.properties);
}

void ClientSession::disconnectFromLogin(const std::string& miniMessage) {
    const wire::text::Component reason = wire::text::miniMessage::parse(miniMessage);
    sendUncompressed(login::LoginDisconnectPacket::kId, [&reason](wire::io::ByteBuffer& buffer) {
        login::LoginDisconnectPacket{.reason = reason}.write(buffer);
    });
    connection_.close();
}

void ClientSession::refuseVersion(const std::string& name, const mv::ProtocolVerdict::Refused& refusal) {
    logger_.info([&name, &refusal] {
        return "Refused " + name + " on " + mv::protocolVersions::describe(refusal.protocol) + ": " +
               std::string{reasonName(refusal.reason)} + ".";
    });
    disconnectFromLogin(versionRefusalOf(refusal.reason));
}

void ClientSession::handleKey(wire::io::ByteBuffer& body) {
    wire::auth::SessionService* online = options_.onlineMode;
    const std::optional<login::KeyPacket> keyPacket = login::KeyPacket::read(body);
    if (online == nullptr || challenge_.empty() || !pendingUsername_.has_value() || !keyPacket.has_value()) {
        connection_.close();
        return;
    }
    const std::optional<std::vector<std::uint8_t>> decryptedChallenge =
        options_.keyPair->decrypt(keyPacket->encryptedChallenge);
    if (!decryptedChallenge.has_value() || *decryptedChallenge != challenge_) {
        connection_.close();
        return;
    }
    const std::optional<std::vector<std::uint8_t>> secretKey = options_.keyPair->decrypt(keyPacket->encryptedSecret);
    if (!secretKey.has_value()) {
        connection_.close();
        return;
    }
    decryptCipher_ = wire::auth::Cfb8Cipher::create(*secretKey);
    encryptCipher_ = wire::auth::Cfb8Cipher::create(*secretKey);

    const std::string serverHash = wire::auth::sessionHash("", *secretKey, options_.keyPair->publicKeyDer());
    const std::optional<wire::auth::SessionProfile> profile = online->hasJoined(*pendingUsername_, serverHash, "");
    if (!profile.has_value()) {
        connection_.close();
        return;
    }
    finishLogin(profile->id, profile->name, profile->properties);
}

std::optional<wire::text::Component> ClientSession::loginRefusal(Uuid id, const std::string& name) {
    // parity: `PlayerList.canPlayerLogin`, which checks the bans and the whitelist before the
    // connection is allowed to go any further.
    storage::PlayerStore* store = options_.playerStore;
    if (store == nullptr || !store->isOpen()) {
        return std::nullopt;
    }
    storage::AccessRepository& access = store->access();
    const std::optional<storage::BanRecord> ban = access.banOf(id.toString(), storage::BanKind::Player);
    if (ban.has_value()) {
        return wire::text::Component::translatable("multiplayer.disconnect.banned.reason",
                                                   {wire::text::Component::text(ban->reason)});
    }
    const std::optional<std::string> address = remoteAddress();
    if (address.has_value()) {
        const std::optional<storage::BanRecord> ipBan = access.banOf(*address, storage::BanKind::Ip);
        if (ipBan.has_value()) {
            return wire::text::Component::translatable("multiplayer.disconnect.banned_ip.reason",
                                                       {wire::text::Component::text(ipBan->reason)});
        }
    }
    if (options_.whitelistEnforced && options_.whitelistEnforced() && !access.isWhitelisted(id.toString())) {
        logger_.info([&name] { return "Refused " + name + ": not on the whitelist."; });
        return wire::text::Component::translatable("multiplayer.disconnect.not_whitelisted");
    }
    return std::nullopt;
}

void ClientSession::finishLogin(Uuid id, const std::string& name,
                                const std::vector<wire::auth::SessionProfileProperty>& properties) {
    const std::optional<wire::text::Component> refusal = loginRefusal(id, name);
    if (refusal.has_value()) {
        sendUncompressed(login::LoginDisconnectPacket::kId, [&refusal](wire::io::ByteBuffer& buffer) {
            login::LoginDisconnectPacket{.reason = *refusal}.write(buffer);
        });
        connection_.close();
        return;
    }
    const std::int32_t threshold = options_.compressionThreshold;
    sendUncompressed(login::LoginCompressionPacket::kId, [threshold](wire::io::ByteBuffer& buffer) {
        login::LoginCompressionPacket{.threshold = threshold}.write(buffer);
    });
    compressionEnabled_ = true;

    std::vector<login::GameProfileProperty> wireProperties;
    wireProperties.reserve(properties.size());
    for (const wire::auth::SessionProfileProperty& property : properties) {
        wireProperties.push_back(login::GameProfileProperty{
            .name = property.name, .value = property.value, .signature = property.signature});
    }
    pendingProfile_ = wire::auth::SessionProfile{.id = id, .name = name, .properties = properties};
    const login::GameProfile profile{.id = id, .name = name, .properties = std::move(wireProperties)};
    const Uuid sessionId = Uuid::offlinePlayer("session:" + name);
    sendCompressed(login::LoginFinishedPacket::kId, [&profile, sessionId](wire::io::ByteBuffer& buffer) {
        login::LoginFinishedPacket{.gameProfile = profile, .sessionId = sessionId}.write(buffer);
    });
    // Before 1.20.2 there is no configuration phase and nothing to acknowledge: the client is
    // already in play. The server still walks its own configuration, whose packets the translation
    // turns into the one thing that client does read, but it walks it by itself.
    if (handshake_.has_value() && handshake_->protocolVersion < mv::protocolVersions::kV1_20_2) {
        enterConfiguration();
    }
}

void ClientSession::enterConfiguration() {
    state_ = wire::protocol::ProtocolState::Configuration;
    // parity: `ServerConfigurationPacketListenerImpl` opens with the brand, which is what F3 prints
    // as `"<brand>" server`. Without it the client has nothing to name us with.
    sendCompressed(wire::protocol::configuration::BrandPacket::kId, [](wire::io::ByteBuffer& buffer) {
        wire::protocol::configuration::BrandPacket{.brand = "Basalt " + std::string{buildInfo::kMinecraftVersion}}
            .write(buffer);
    });
    // Claims the client's own vanilla data pack rather than an empty list. A client older than
    // 1.20.5 has no such packet and would never answer, so it is not asked.
    if (handshake_.has_value() && handshake_->protocolVersion < mv::protocolVersions::kV1_20_5) {
        sendRegistriesThenFinish();
        return;
    }
    sendCompressed(
        wire::protocol::configuration::SelectKnownPacksPacket::kClientboundId, [](wire::io::ByteBuffer& buffer) {
            wire::protocol::configuration::SelectKnownPacksPacket{
                .packs = {wire::protocol::configuration::KnownPack{
                    .namespaceName = "minecraft", .id = "core", .version = std::string{buildInfo::kMinecraftVersion}}}}
                .write(buffer);
        });
}

void ClientSession::handleConfiguration(std::int32_t packetId, wire::io::ByteBuffer& body) {
    if (packetId == wire::protocol::configuration::SelectKnownPacksPacket::kServerboundId) {
        sendRegistriesThenFinish();
        return;
    }
    if (packetId == wire::protocol::configuration::kFinishConfigurationId) {
        enterPlay();
        return;
    }
    if (packetId == wire::protocol::configuration::ClientInformationPacket::kId) {
        // Read even here, before the player exists: it is where the skin layer mask, the chat
        // setting and the view distance first arrive.
        const std::optional<wire::protocol::configuration::ClientInformationPacket> information =
            wire::protocol::configuration::ClientInformationPacket::read(body);
        if (information.has_value()) {
            pendingSkinParts_ = static_cast<std::int8_t>(information->skinParts);
            pendingChatVisibility_ = information->chatFlags;
            pendingViewDistance_ = information->viewDistance;
        }
    }
}

std::vector<std::int32_t> ClientSession::tagMembers(std::string_view registryId, std::string_view tagName) const {
    // Members are registry ids of the tagged thing: a block tag carries `minecraft:block` ids, not
    // block *state* ids, which are a different numbering entirely.
    std::vector<std::int32_t> ids;
    if (registryId == "minecraft:block" && options_.blockTags != nullptr) {
        for (const Identifier& member : world::realBlockTags::membersOf(*options_.blockTags, tagName)) {
            const std::optional<std::int32_t> id = reg::blocks::blockIdOf(member);
            if (id.has_value()) {
                ids.push_back(*id);
            }
        }
        return ids;
    }
    if (registryId == "minecraft:item" && options_.itemTags != nullptr) {
        for (const Identifier& member : world::realItemTags::membersOf(*options_.itemTags, tagName)) {
            const reg::Item* item = reg::items::registry().byName(member);
            if (item != nullptr) {
                ids.push_back(item->protocolId);
            }
        }
        return ids;
    }
    const std::optional<Identifier> tagId = ([tagName] {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(std::string{tagName});
        return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
    })();
    if (!tagId.has_value()) {
        return ids;
    }
    if (registryId == "minecraft:fluid" && options_.fluidTags != nullptr) {
        for (const Identifier& member : world::realFluidTags::membersOf(*options_.fluidTags, *tagId)) {
            const std::optional<std::int32_t> id = ([&member] {
                const auto& ids = world::realFluidTags::protocolIds();
                const auto found = ids.find(member);
                return found == ids.end() ? std::nullopt : std::optional{found->second};
            })();
            if (id.has_value()) {
                ids.push_back(*id);
            }
        }
        return ids;
    }
    // The ids are indices into the timeline registry this server sent, in that same order.
    if (registryId == "minecraft:timeline" && options_.timelineTags != nullptr && options_.timelines != nullptr) {
        const std::vector<Identifier> order = options_.timelines->ids();
        for (const Identifier& member : world::realTimelineTags::membersOf(*options_.timelineTags, *tagId)) {
            const auto found = std::ranges::find(order, member);
            if (found != order.end()) {
                ids.push_back(static_cast<std::int32_t>(std::distance(order.begin(), found)));
            }
        }
        return ids;
    }
    if (registryId == "minecraft:entity_type" && options_.entityTypeTags != nullptr) {
        for (const Identifier& member : world::realEntityTypeTags::membersOf(*options_.entityTypeTags, *tagId)) {
            const reg::EntityTypeEntry* type = reg::entityTypes::of(member);
            if (type != nullptr) {
                ids.push_back(type->protocolId);
            }
        }
    }
    return ids;
}

void ClientSession::sendRegistriesThenFinish() {
    using wire::protocol::configuration::RegistryDataPacket;
    using wire::protocol::configuration::RegistryEntry;

    for (const RegistryDataPacket& packet : registry::vanillaConfigurationRegistries::packets()) {
        sendCompressed(RegistryDataPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
    // The rest of `RegistryDataLoader.SYNCHRONIZED_REGISTRIES`, sent empty: a registry that was
    // never sent does not exist client-side, and tags scoped to it stop resolving. Enchantments are
    // all or nothing, because a partial registry disconnects at login.
    const auto sendLoaded = [this](std::string_view id, const auto& entries) {
        if (entries.empty()) {
            return false;
        }
        std::vector<RegistryEntry> written;
        written.reserve(entries.size());
        for (const auto& [name, value] : entries) {
            written.push_back(RegistryEntry{.key = name.toString(), .value = wire::nbt::NbtTag::ofCompound(value)});
        }
        sendCompressed(RegistryDataPacket::kId, [id, &written](wire::io::ByteBuffer& buffer) {
            RegistryDataPacket{.registryId = std::string{id}, .entries = written}.write(buffer);
        });
        return true;
    };
    const bool sentEnchantments =
        options_.enchantments != nullptr && sendLoaded("minecraft:enchantment", options_.enchantments->entries());
    const bool sentDialogs = options_.dialogs != nullptr && sendLoaded("minecraft:dialog", options_.dialogs->entries());
    // Without these the client has a clock that ticks and nothing that reads it: the sky never moves
    // however much `/time` reports success.
    const bool sentTimelines =
        options_.timelines != nullptr && sendLoaded("minecraft:timeline", options_.timelines->entries());
    for (const std::string_view emptyRegistryId : kEmptySynchronizedRegistries) {
        if ((emptyRegistryId == "minecraft:enchantment" && sentEnchantments) ||
            (emptyRegistryId == "minecraft:dialog" && sentDialogs) ||
            (emptyRegistryId == "minecraft:timeline" && sentTimelines)) {
            continue;
        }
        sendCompressed(RegistryDataPacket::kId, [emptyRegistryId](wire::io::ByteBuffer& buffer) {
            RegistryDataPacket{.registryId = std::string{emptyRegistryId}, .entries = {}}.write(buffer);
        });
    }
    // Every tag name the jar ships goes out: the client looks tags up by key whether or not their
    // registry is loaded. Members matter separately, because it runs its own tool rules against them
    // and an empty `mineable/pickaxe` means bare-hand speed.
    std::vector<wire::protocol::configuration::TagGroup> groups;
    for (const auto& [registryId, tagNames] : registry::vanillaTagData::tagsByRegistry()) {
        std::vector<wire::protocol::configuration::TagEntry> entries;
        entries.reserve(tagNames.size());
        for (const std::string_view tagName : tagNames) {
            entries.push_back(wire::protocol::configuration::TagEntry{.name = std::string{tagName},
                                                                      .ids = tagMembers(registryId, tagName)});
        }
        groups.push_back(
            wire::protocol::configuration::TagGroup{.registryId = std::string{registryId}, .tags = std::move(entries)});
    }
    sendCompressed(wire::protocol::configuration::TagsPacket::kId, [&groups](wire::io::ByteBuffer& buffer) {
        wire::protocol::configuration::TagsPacket{.groups = groups}.write(buffer);
    });
    sendCompressed(wire::protocol::configuration::kFinishConfigurationId, [](wire::io::ByteBuffer&) {});
    // Same again at the other end of it: a client with no configuration phase will not answer that
    // this one is over.
    if (handshake_.has_value() && handshake_->protocolVersion < mv::protocolVersions::kV1_20_2) {
        enterPlay();
    }
}

} // namespace basalt::server::runtime
