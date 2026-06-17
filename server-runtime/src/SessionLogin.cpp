#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/registry/PackRegistries.hpp>
#include <basalt/server/runtime/registry/SyncedTags.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaTagData.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/login/LoginPluginPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace login = wire::protocol::login;
namespace mv = wire::multiversion;

constexpr std::int32_t kConfigurationCustomPayloadId = 0x02;
namespace reg = server::registry;

using wire::common::Identifier;
using wire::common::Uuid;

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

// Sent empty so the client's registry access has an entry, which tags scoped to them need to resolve.
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
        connection_.close();
        return;
    }
    // Read now, acted on at login: a refusal there is a disconnect screen, not a silent close.
    if (proxy_.readsHandshake() && packet->intent == wire::protocol::handshake::ClientIntent::Login) {
        forwardedHandshake_ = proxy_.readHandshake(packet->hostName);
        if (const auto* accepted = std::get_if<wire::auth::forwarding::ForwardedLogin>(&*forwardedHandshake_);
            accepted != nullptr) {
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
        // A client paints the entry red when its number differs, so echoing an accepted client's own avoids that.
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
    // A proxy already authenticated this player, so what it says is their whole identity.
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
    // The one gate before a player exists: a plugin saying no means they never join.
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
    // parity: `PlayerList.canPlayerLogin`.
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
    // The proxy that selected the envelope sends its Open next; the client's own packets follow inside it.
    if (proxy_.login().has_value() &&
        (proxy_.login()->capabilities & wire::auth::forwarding::native::kCapabilitySessionEnvelope) != 0) {
        envelope_.emplace();
        return;
    }
    // Before 1.20.2 there is no configuration phase to acknowledge: the server walks its own by itself.
    if (handshake_.has_value() && handshake_->protocolVersion < mv::protocolVersions::kV1_20_2) {
        enterConfiguration();
    }
}

void ClientSession::sendServerLinks(const std::vector<server::plugins::PluginServerLink>& links) {
    static constexpr std::array<std::string_view, 10> kKinds{
        "bug_report", "community_guidelines", "support", "status", "feedback", "community", "website", "forums",
        "news",       "announcements"};
    std::vector<const server::plugins::PluginServerLink*> shown;
    for (const server::plugins::PluginServerLink& link : links) {
        if (link.url.starts_with("http://") || link.url.starts_with("https://")) {
            shown.push_back(&link);
        }
    }
    if (shown.empty()) {
        return;
    }
    sendCompressed(wire::protocol::configuration::ServerLinksPacket::kId, [&shown](wire::io::ByteBuffer& buffer) {
        wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(shown.size()));
        for (const server::plugins::PluginServerLink* link : shown) {
            const auto kind = std::ranges::find(kKinds, link->kind);
            if (kind != kKinds.end()) {
                wire::protocol::writeBoolean(buffer, true);
                wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(kind - kKinds.begin()));
            } else {
                wire::protocol::writeBoolean(buffer, false);
                const auto parsed = wire::common::parseJson(link->label);
                wire::protocol::writeAnonymousNbt(
                    buffer, wire::text::componentNbt::serialize(parsed.has_value()
                                                                    ? wire::text::componentJson::deserialize(*parsed)
                                                                    : wire::text::Component::text(link->label)));
            }
            wire::protocol::writeString(buffer, link->url);
        }
    });
}

void ClientSession::enterConfiguration() {
    state_ = wire::protocol::ProtocolState::Configuration;
    // parity: `ServerConfigurationPacketListenerImpl`.
    sendCompressed(wire::protocol::configuration::BrandPacket::kId, [](wire::io::ByteBuffer& buffer) {
        wire::protocol::configuration::BrandPacket{.brand = "Basalt " + std::string{buildInfo::kMinecraftVersion}}
            .write(buffer);
    });
    // parity: `ServerLinks`; `Util.parseAndValidateUntrustedUri`.
    const bool serverLinksKnown = handshake_.has_value() && handshake_->protocolVersion == buildInfo::kProtocolVersion;
    if (serverLinksKnown && options_.serverLinks) {
        sendServerLinks(options_.serverLinks());
    } else if (serverLinksKnown &&
               (options_.bugReportLink.starts_with("http://") || options_.bugReportLink.starts_with("https://"))) {
        sendCompressed(wire::protocol::configuration::ServerLinksPacket::kId, [this](wire::io::ByteBuffer& buffer) {
            wire::protocol::configuration::ServerLinksPacket{
                .links = {{wire::protocol::configuration::kBugReportLinkType, options_.bugReportLink}}}
                .write(buffer);
        });
    }
    // parity: `ClientboundUpdateEnabledFeaturesPacket`.
    if (handshake_.has_value() && handshake_->protocolVersion == buildInfo::kProtocolVersion) {
        std::vector<std::string> features{"minecraft:vanilla"};
        if (options_.enabledFeatures) {
            features = options_.enabledFeatures();
        }
        sendCompressed(
            wire::protocol::configuration::UpdateEnabledFeaturesPacket::kId, [&features](wire::io::ByteBuffer& buffer) {
                wire::protocol::configuration::UpdateEnabledFeaturesPacket{.features = features}.write(buffer);
            });
    }
    // A client older than 1.20.5 has no such packet and would never answer, so it is not asked.
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
    if (packetId == wire::protocol::configuration::ResourcePackResponsePacket::kConfigurationId) {
        if (const auto response = wire::protocol::configuration::ResourcePackResponsePacket::read(body);
            response.has_value()) {
            handleResourcePackResponse(*response, true);
        }
        return;
    }
    if (packetId == wire::protocol::configuration::kFinishConfigurationId) {
        enterPlay();
        return;
    }
    if (packetId == wire::protocol::configuration::ClientInformationPacket::kId) {
        // Read even before the player exists: skin layer mask, chat setting and view distance first arrive here.
        const std::optional<wire::protocol::configuration::ClientInformationPacket> information =
            wire::protocol::configuration::ClientInformationPacket::read(body);
        if (information.has_value()) {
            pendingSkinParts_ = static_cast<std::int8_t>(information->skinParts);
            pendingMainHand_ = information->mainHand;
            pendingChatVisibility_ = information->chatFlags;
            pendingViewDistance_ = information->viewDistance;
            pendingOptions_ = player::ClientOptions{.locale = information->locale,
                                                    .viewDistance = information->viewDistance,
                                                    .particleStatus = information->particleStatus,
                                                    .chatColors = information->chatColors,
                                                    .textFiltering = information->enableTextFiltering,
                                                    .serverListings = information->enableServerListing};
        }
        return;
    }
    if (packetId == kConfigurationCustomPayloadId) {
        const std::optional<wire::protocol::play::CustomPayloadPacket> payload =
            wire::protocol::play::CustomPayloadPacket::read(body);
        if (payload.has_value() && payload->channel == wire::protocol::play::CustomPayloadPacket::kBrandChannel) {
            wire::io::ByteBuffer data;
            data.writeBytes(payload->data);
            pendingBrand_ = wire::protocol::readString(data, 64);
        }
    }
}

void ClientSession::sendRegistriesThenFinish() {
    using wire::protocol::configuration::RegistryDataPacket;
    using wire::protocol::configuration::RegistryEntry;

    for (const RegistryDataPacket& packet : registry::installedRegistries()) {
        sendCompressed(RegistryDataPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
    // A registry never sent does not exist client-side and its tags stop resolving; enchantments are all or nothing.
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
    // Without these the client has a clock that ticks and nothing that reads it: the sky never moves.
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
    // Every tag name goes out; members matter too, since an empty `mineable/pickaxe` means bare-hand speed.
    const std::vector<wire::protocol::configuration::TagGroup> groups =
        registry::syncedTags(registry::TagSources{.blocks = options_.blockTags,
                                                  .items = options_.itemTags,
                                                  .fluids = options_.fluidTags,
                                                  .entityTypes = options_.entityTypeTags,
                                                  .timelines = options_.timelineTags,
                                                  .timelineOrder = options_.timelines});
    sendCompressed(wire::protocol::configuration::TagsPacket::kId, [&groups](wire::io::ByteBuffer& buffer) {
        wire::protocol::configuration::TagsPacket{.groups = groups}.write(buffer);
    });
    // parity: `ServerResourcePackConfigurationTask`, which holds the next task until the client answers for good.
    if (options_.resourcePack.has_value() && handshake_.has_value() &&
        handshake_->protocolVersion == buildInfo::kProtocolVersion) {
        const Options::ResourcePack& pack = *options_.resourcePack;
        awaitingResourcePack_ = true;
        sendCompressed(
            wire::protocol::configuration::ResourcePackPushPacket::kConfigurationId,
            [&pack](wire::io::ByteBuffer& buffer) {
                wire::protocol::configuration::ResourcePackPushPacket{
                    .id = pack.id, .url = pack.url, .hash = pack.hash, .required = pack.required, .prompt = pack.prompt}
                    .write(buffer);
            });
        return;
    }
    finishConfiguration();
}

void ClientSession::handleResourcePackResponse(
    const wire::protocol::configuration::ResourcePackResponsePacket& response, bool configuring) {
    // parity: `ServerCommonPacketListenerImpl.handleResourcePackResponse`.
    if (playerSession_ != nullptr) {
        playerSession_->extras.packStatus = response.action;
    }
    if (response.action == wire::protocol::configuration::ResourcePackResponsePacket::kDeclined &&
        options_.resourcePack.has_value() && options_.resourcePack->required) {
        const wire::text::Component reason =
            wire::text::Component::translatable("multiplayer.requiredTexturePrompt.disconnect");
        if (configuring) {
            sendCompressed(wire::protocol::configuration::ConfigurationDisconnectPacket::kId,
                           [&reason](wire::io::ByteBuffer& buffer) {
                               wire::protocol::configuration::ConfigurationDisconnectPacket{.reason = reason}.write(
                                   buffer);
                           });
            closeWhenDrained_.store(true);
            if (!outbox_.isOpen()) {
                outbox_.close();
                connection_.close();
            }
        } else {
            disconnect(reason);
        }
        return;
    }
    if (configuring && awaitingResourcePack_ && response.terminal()) {
        awaitingResourcePack_ = false;
        finishConfiguration();
    }
}

void ClientSession::finishConfiguration() {
    sendCompressed(wire::protocol::configuration::kFinishConfigurationId, [](wire::io::ByteBuffer&) {});
    // Same at the other end: a client with no configuration phase will not answer that this one is over.
    if (handshake_.has_value() && handshake_->protocolVersion < mv::protocolVersions::kV1_20_2) {
        enterPlay();
    }
}

} // namespace basalt::server::runtime
