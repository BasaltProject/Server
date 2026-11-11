// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import com.google.common.primitives.Ints;
import com.mojang.authlib.GameProfile;
import java.math.BigInteger;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.security.PrivateKey;
import java.util.concurrent.atomic.AtomicInteger;
import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.core.UUIDUtil;
import net.minecraft.network.Connection;
import net.minecraft.network.DisconnectionDetails;
import net.minecraft.network.TickablePacketListener;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.configuration.ConfigurationProtocols;
import net.minecraft.network.protocol.cookie.ServerboundCookieResponsePacket;
import net.minecraft.network.protocol.login.ClientboundHelloPacket;
import net.minecraft.network.protocol.login.ClientboundLoginCompressionPacket;
import net.minecraft.network.protocol.login.ClientboundLoginDisconnectPacket;
import net.minecraft.network.protocol.login.ClientboundLoginFinishedPacket;
import net.minecraft.network.protocol.login.ServerLoginPacketListener;
import net.minecraft.network.protocol.login.ServerboundCustomQueryAnswerPacket;
import net.minecraft.network.protocol.login.ServerboundHelloPacket;
import net.minecraft.network.protocol.login.ServerboundKeyPacket;
import net.minecraft.network.protocol.login.ServerboundLoginAcknowledgedPacket;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.PlayerList;
import net.minecraft.util.Crypt;
import net.minecraft.util.CryptException;
import net.minecraft.util.StringUtil;
import net.minecraft.world.entity.player.Player;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class ServerLoginPacketListenerImpl implements ServerLoginPacketListener, TickablePacketListener {
    private static final Logger LOG = LoggerFactory.getLogger(ServerLoginPacketListenerImpl.class);
    private static final AtomicInteger AUTH_THREADS = new AtomicInteger();
    private static final int LOGIN_TIMEOUT_TICKS = 600;
    private final byte[] challenge = Ints.toByteArray(new java.util.Random().nextInt());
    private final MinecraftServer server;
    private final Connection connection;
    private final boolean transferred;
    private volatile State state = State.HELLO;
    private int ticks;
    private String requestedUsername;
    private GameProfile authenticatedProfile;

    public ServerLoginPacketListenerImpl(MinecraftServer minecraftserver, Connection connection, boolean transferred) {
        this.server = minecraftserver;
        this.connection = connection;
        this.transferred = transferred;
    }

    public void tick() {
        if (state == State.VERIFYING) {
            verifyLoginAndFinishConnectionSetup(authenticatedProfile);
        }
        if (state == State.WAITING_FOR_DUPE_DISCONNECT && server.getPlayerList().getPlayer(authenticatedProfile.id()) == null) {
            finishLoginAndWaitForClient(authenticatedProfile);
        }
        if (ticks++ == LOGIN_TIMEOUT_TICKS) {
            disconnect(Component.translatable("multiplayer.disconnect.slow_login"));
        }
    }

    public boolean isAcceptingMessages() {
        return connection.isConnected();
    }

    public void disconnect(Component component) {
        try {
            LOG.info("Disconnecting {}: {}", getUserName(), component.getString());
            connection.send(new ClientboundLoginDisconnectPacket(component));
            connection.disconnect(component);
        } catch (Exception failure) {
            LOG.error("Error whilst disconnecting player", failure);
        }
    }

    public void onDisconnect(DisconnectionDetails details) {
        LOG.info("{} lost connection: {}", getUserName(), details.reason().getString());
    }

    public String getUserName() {
        String address = connection.getLoggableAddress(server.logIPs());
        return requestedUsername == null ? address : requestedUsername + " (" + address + ")";
    }

    private static void require(boolean condition, String message) {
        if (!condition) {
            throw new IllegalStateException(message);
        }
    }

    public void handleHello(ServerboundHelloPacket packet) {
        require(state == State.HELLO, "Unexpected hello packet");
        require(StringUtil.isValidPlayerName(packet.name()), "Invalid characters in username");
        requestedUsername = packet.name();
        GameProfile owner = server.getSingleplayerProfile();
        if (owner != null && requestedUsername.equalsIgnoreCase(owner.name())) {
            startClientVerification(owner);
        } else if (server.usesAuthentication() && !connection.isMemoryConnection()) {
            state = State.KEY;
            connection.send(new ClientboundHelloPacket("", server.getKeyPair().getPublic().getEncoded(), challenge, true));
        } else {
            startClientVerification(UUIDUtil.createOfflineProfile(requestedUsername));
        }
    }

    private void startClientVerification(GameProfile profile) {
        authenticatedProfile = profile;
        state = State.VERIFYING;
    }

    private void verifyLoginAndFinishConnectionSetup(GameProfile profile) {
        PlayerList players = server.getPlayerList();
        Component refusal = players.canPlayerLogin(connection.getRemoteAddress(), new NameAndId(profile));
        if (refusal != null) {
            disconnect(refusal);
            return;
        }
        int threshold = server.getCompressionThreshold();
        if (threshold >= 0 && !connection.isMemoryConnection()) {
            connection.send(new ClientboundLoginCompressionPacket(threshold));
            connection.setupCompression(threshold, true);
        }
        if (players.disconnectAllPlayersWithProfile(profile.id())) {
            state = State.WAITING_FOR_DUPE_DISCONNECT;
        } else {
            finishLoginAndWaitForClient(profile);
        }
    }

    private void finishLoginAndWaitForClient(GameProfile profile) {
        state = State.PROTOCOL_SWITCHING;
        connection.send(new ClientboundLoginFinishedPacket(profile, server.getConnection().getSessionId()));
    }

    public void handleKey(ServerboundKeyPacket packet) {
        require(state == State.KEY, "Unexpected key packet");
        String digest;
        try {
            PrivateKey priv = server.getKeyPair().getPrivate();
            if (!packet.isChallengeValid(challenge, priv)) {
                throw new IllegalStateException("Protocol error");
            }
            SecretKey secret = packet.getSecretKey(priv);
            digest = new BigInteger(Crypt.digestData("", server.getKeyPair().getPublic(), secret)).toString(16);
            state = State.AUTHENTICATING;
            Cipher decrypt = Crypt.getCipher(2, secret);
            Cipher encrypt = Crypt.getCipher(1, secret);
            connection.setEncryptionKey(decrypt, encrypt);
        } catch (CryptException bad) {
            throw new IllegalStateException("Protocol error", bad);
        }
        String name = requestedUsername;
        Thread authenticator = new Thread(() -> authenticate(name, digest), "User Authenticator #" + AUTH_THREADS.incrementAndGet());
        authenticator.setUncaughtExceptionHandler((thread, failure) -> LOG.error("Uncaught exception in {}", thread.getName(), failure));
        authenticator.start();
    }

    private void authenticate(String name, String digest) {
        try {
            GameProfile profile = hasJoinedServer(name, digest, clientAddress());
            if (profile != null) {
                LOG.info("UUID of player {} is {}", profile.name(), profile.id());
                server.getServerActivityMonitor().reportLoginActivity();
                startClientVerification(profile);
            } else if (server.isSingleplayer()) {
                LOG.warn("Failed to verify username but will let them in anyway!");
                startClientVerification(UUIDUtil.createOfflineProfile(name));
            } else {
                disconnect(Component.translatable("multiplayer.disconnect.unverified_username"));
                LOG.error("Username '{}' tried to join with an invalid session", name);
            }
        } catch (com.mojang.authlib.exceptions.AuthenticationUnavailableException down) {
            if (server.isSingleplayer()) {
                LOG.warn("Authentication servers are down but will let them in anyway!");
                startClientVerification(UUIDUtil.createOfflineProfile(name));
            } else {
                disconnect(Component.translatable("multiplayer.disconnect.authservers_down"));
                LOG.error("Couldn't verify username because servers are unavailable");
            }
        }
    }

    private static GameProfile hasJoinedServer(String name, String serverId, InetAddress address)
        throws com.mojang.authlib.exceptions.AuthenticationUnavailableException {
        StringBuilder url = new StringBuilder("https://sessionserver.mojang.com/session/minecraft/hasJoined?username=")
            .append(java.net.URLEncoder.encode(name, java.nio.charset.StandardCharsets.UTF_8)).append("&serverId=").append(serverId);
        if (address != null) {
            url.append("&ip=").append(java.net.URLEncoder.encode(address.getHostAddress(), java.nio.charset.StandardCharsets.UTF_8));
        }
        try {
            java.net.http.HttpResponse<String> reply = java.net.http.HttpClient.newHttpClient().send(
                java.net.http.HttpRequest.newBuilder(java.net.URI.create(url.toString())).GET().build(),
                java.net.http.HttpResponse.BodyHandlers.ofString());
            if (reply.statusCode() == 204) {
                return null;
            }
            if (reply.statusCode() != 200) {
                throw new com.mojang.authlib.exceptions.AuthenticationUnavailableException("Session server answered " + reply.statusCode());
            }
            com.google.gson.JsonObject body = com.google.gson.JsonParser.parseString(reply.body()).getAsJsonObject();
            return new GameProfile(java.util.UUID.fromString(body.get("id").getAsString()
                .replaceFirst("(\\p{XDigit}{8})(\\p{XDigit}{4})(\\p{XDigit}{4})(\\p{XDigit}{4})(\\p{XDigit}+)", "$1-$2-$3-$4-$5")),
                body.get("name").getAsString());
        } catch (java.io.IOException | InterruptedException | RuntimeException failure) {
            throw new com.mojang.authlib.exceptions.AuthenticationUnavailableException("Session server unreachable", failure);
        }
    }

    private InetAddress clientAddress() {
        SocketAddress remote = connection.getRemoteAddress();
        return server.getPreventProxyConnections() && remote instanceof InetSocketAddress inet ? inet.getAddress() : null;
    }

    public void handleCustomQueryPacket(ServerboundCustomQueryAnswerPacket packet) {
        disconnect(ServerCommonPacketListenerImpl.DISCONNECT_UNEXPECTED_QUERY);
    }

    public void handleLoginAcknowledgement(ServerboundLoginAcknowledgedPacket packet) {
        require(state == State.PROTOCOL_SWITCHING, "Unexpected login acknowledgement packet");
        connection.setupOutboundProtocol(ConfigurationProtocols.CLIENTBOUND);
        CommonListenerCookie cookie = CommonListenerCookie.createInitial(authenticatedProfile, transferred);
        ServerConfigurationPacketListenerImpl configuration = new ServerConfigurationPacketListenerImpl(server, connection, cookie);
        connection.setupInboundProtocol(ConfigurationProtocols.SERVERBOUND, configuration);
        configuration.startConfiguration();
        state = State.ACCEPTED;
    }

    public void fillListenerSpecificCrashDetails(CrashReport report, CrashReportCategory connectionDetails) {
        connectionDetails.setDetail("Login phase", () -> state.toString());
    }

    public void handleCookieResponse(ServerboundCookieResponsePacket packet) {
        disconnect(ServerCommonPacketListenerImpl.DISCONNECT_UNEXPECTED_QUERY);
    }

    static enum State {
        HELLO, KEY, AUTHENTICATING, NEGOTIATING, VERIFYING, WAITING_FOR_DUPE_DISCONNECT, PROTOCOL_SWITCHING, ACCEPTED;
    }
}
