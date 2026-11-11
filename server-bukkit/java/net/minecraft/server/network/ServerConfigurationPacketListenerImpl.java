// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import com.mojang.authlib.GameProfile;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.network.Connection;
import net.minecraft.network.DisconnectionDetails;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.TickablePacketListener;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.PacketUtils;
import net.minecraft.network.protocol.common.ClientboundCustomPayloadPacket;
import net.minecraft.network.protocol.common.ClientboundServerLinksPacket;
import net.minecraft.network.protocol.common.ServerboundClientInformationPacket;
import net.minecraft.network.protocol.common.ServerboundResourcePackPacket;
import net.minecraft.network.protocol.common.custom.BrandPayload;
import net.minecraft.network.protocol.configuration.ClientboundUpdateEnabledFeaturesPacket;
import net.minecraft.network.protocol.configuration.ServerConfigurationPacketListener;
import net.minecraft.network.protocol.configuration.ServerboundAcceptCodeOfConductPacket;
import net.minecraft.network.protocol.configuration.ServerboundFinishConfigurationPacket;
import net.minecraft.network.protocol.configuration.ServerboundSelectKnownPacks;
import net.minecraft.network.protocol.game.GameProtocols;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.RegistryLayer;
import net.minecraft.server.ServerLinks;
import net.minecraft.server.level.ClientInformation;
import net.minecraft.server.network.config.JoinWorldTask;
import net.minecraft.server.network.config.PrepareSpawnTask;
import net.minecraft.server.network.config.ServerCodeOfConductConfigurationTask;
import net.minecraft.server.network.config.ServerResourcePackConfigurationTask;
import net.minecraft.server.network.config.SynchronizeRegistriesTask;
import net.minecraft.server.packs.repository.KnownPack;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.PlayerList;
import net.minecraft.world.flag.FeatureFlags;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class ServerConfigurationPacketListenerImpl extends ServerCommonPacketListenerImpl
    implements ServerConfigurationPacketListener, TickablePacketListener {
    private static final Logger LOG = LoggerFactory.getLogger(ServerConfigurationPacketListenerImpl.class);
    private static final Component INVALID_DATA = Component.translatable("multiplayer.disconnect.invalid_player_data");
    private static final Component CONFIGURATION_ERROR = Component.translatable("multiplayer.disconnect.configuration_error");
    private final GameProfile gameProfile;
    private final Queue<ConfigurationTask> queue = new ConcurrentLinkedQueue<>();
    private ConfigurationTask current;
    private ClientInformation clientInformation;
    private SynchronizeRegistriesTask registrySync;
    private PrepareSpawnTask spawnPrep;

    public ServerConfigurationPacketListenerImpl(MinecraftServer server, Connection connection, CommonListenerCookie cookie) {
        super(server, connection, cookie);
        this.gameProfile = cookie.gameProfile();
        this.clientInformation = cookie.clientInformation();
    }

    protected GameProfile playerProfile() {
        return gameProfile;
    }

    public void onDisconnect(DisconnectionDetails details) {
        LOG.info("{} ({}) lost connection: {}", gameProfile.name(), gameProfile.id(), details.reason().getString());
        if (spawnPrep != null) {
            spawnPrep.close();
            spawnPrep = null;
        }
        super.onDisconnect(details);
    }

    public boolean isAcceptingMessages() {
        return connection.isConnected();
    }

    public void startConfiguration() {
        send(new ClientboundCustomPayloadPacket(new BrandPayload(server.getServerModName())));
        ServerLinks links = server.serverLinks();
        if (!links.isEmpty()) {
            send(new ClientboundServerLinksPacket(links.untrust()));
        }
        LayeredRegistryAccess<RegistryLayer> registries = server.registries();
        List<KnownPack> known = server.getResourceManager().listPacks()
            .flatMap(pack -> pack.location().knownPackInfo().stream()).toList();
        send(new ClientboundUpdateEnabledFeaturesPacket(FeatureFlags.REGISTRY.toNames(server.getWorldData().enabledFeatures())));
        registrySync = new SynchronizeRegistriesTask(known, registries);
        queue.add(registrySync);
        addOptionalTasks();
        returnToWorld();
    }

    public void returnToWorld() {
        spawnPrep = new PrepareSpawnTask(server, new NameAndId(gameProfile));
        queue.add(spawnPrep);
        queue.add(new JoinWorldTask());
        startNextTask();
    }

    private void addOptionalTasks() {
        Map<String, String> conducts = server.getCodeOfConducts();
        if (!conducts.isEmpty()) {
            queue.add(new ServerCodeOfConductConfigurationTask(() -> {
                String text = conducts.get(clientInformation.language().toLowerCase(Locale.ROOT));
                if (text == null) {
                    text = conducts.get("en_us");
                }
                return text != null ? text : conducts.values().iterator().next();
            }));
        }
        server.getServerResourcePack().ifPresent(pack -> queue.add(new ServerResourcePackConfigurationTask(pack)));
    }

    public void handleClientInformation(ServerboundClientInformationPacket packet) {
        clientInformation = packet.information();
    }

    public void handleResourcePackResponse(ServerboundResourcePackPacket packet) {
        super.handleResourcePackResponse(packet);
        if (packet.action().isTerminal()) {
            finishCurrentTask(ServerResourcePackConfigurationTask.TYPE);
        }
    }

    public void handleSelectKnownPacks(ServerboundSelectKnownPacks packet) {
        PacketUtils.ensureRunningOnSameThread(packet, this, server.packetProcessor());
        if (registrySync == null) {
            throw new IllegalStateException("Unexpected response from client: received pack selection, but no negotiation ongoing");
        }
        registrySync.handleResponse(packet.knownPacks(), this::send);
        finishCurrentTask(SynchronizeRegistriesTask.TYPE);
    }

    public void handleAcceptCodeOfConduct(ServerboundAcceptCodeOfConductPacket packet) {
        finishCurrentTask(ServerCodeOfConductConfigurationTask.TYPE);
    }

    public void handleConfigurationFinished(ServerboundFinishConfigurationPacket packet) {
        PacketUtils.ensureRunningOnSameThread(packet, this, server.packetProcessor());
        finishCurrentTask(JoinWorldTask.TYPE);
        connection.setupOutboundProtocol(GameProtocols.CLIENTBOUND_TEMPLATE.bind(RegistryFriendlyByteBuf.decorator(server.registryAccess())));
        try {
            PlayerList players = server.getPlayerList();
            if (players.getPlayer(gameProfile.id()) != null) {
                disconnect(PlayerList.DUPLICATE_LOGIN_DISCONNECT_MESSAGE);
                return;
            }
            Component refusal = players.canPlayerLogin(connection.getRemoteAddress(), new NameAndId(gameProfile));
            if (refusal != null) {
                disconnect(refusal);
                return;
            }
            spawnPrep.spawnPlayer(connection, createCookie(clientInformation));
        } catch (Exception failure) {
            LOG.error("Couldn't place player in world", failure);
            disconnect(INVALID_DATA);
        }
    }

    public void tick() {
        keepConnectionAlive();
        ConfigurationTask task = current;
        if (task != null) {
            try {
                if (task.tick()) {
                    finishCurrentTask(task.type());
                }
            } catch (Exception failure) {
                LOG.error("Failed to tick configuration task {}", task.type(), failure);
                disconnect(CONFIGURATION_ERROR);
            }
        }
        if (spawnPrep != null) {
            spawnPrep.keepAlive();
        }
    }

    private void startNextTask() {
        if (current != null) {
            throw new IllegalStateException("Task " + current.type().id() + " has not finished yet");
        }
        if (!isAcceptingMessages()) {
            return;
        }
        ConfigurationTask next = queue.poll();
        if (next == null) {
            return;
        }
        current = next;
        try {
            next.start(this::send);
        } catch (Exception failure) {
            LOG.error("Failed to start configuration task {}", next.type(), failure);
            disconnect(CONFIGURATION_ERROR);
        }
    }

    private void finishCurrentTask(ConfigurationTask.Type finished) {
        ConfigurationTask.Type running = current != null ? current.type() : null;
        if (!finished.equals(running)) {
            throw new IllegalStateException("Unexpected request for task finish, current task: " + running + ", requested: " + finished);
        }
        current = null;
        startNextTask();
    }
}
