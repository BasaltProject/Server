// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.common.net.HostAndPort;
import io.netty.channel.EventLoopGroup;
import io.netty.handler.ssl.SslContext;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.jsonrpc.security.AuthenticationHandler;
import net.minecraft.server.notifications.NotificationManager;
import org.basaltmc.bukkit.WorldEnvNatives;

/** The management protocol is served by the native server on its own port; this class reports and tracks it. */
public class ManagementServer {
    private final HostAndPort hostAndPort;
    private final List<Connection> connections = new CopyOnWriteArrayList<>();

    public ManagementServer(HostAndPort hostAndPort, AuthenticationHandler authenticationHandler) {
        this.hostAndPort = hostAndPort;
    }

    public ManagementServer(HostAndPort hostAndPort, AuthenticationHandler authenticationHandler, EventLoopGroup eventLoopGroup) {
        this(hostAndPort, authenticationHandler);
    }

    public boolean scheduleHeartbeat(NotificationManager notificationManager, long period) {
        return WorldEnvNatives.setSetting("status_heartbeat_interval", String.valueOf(Math.max(0L, period)));
    }

    public void onConnected(Connection connection) {
        connections.add(connection);
    }

    public void onDisconnected(Connection connection) {
        connections.remove(connection);
    }

    public void startWithoutTls(MinecraftApi minecraftApi) {
        requireApi();
    }

    public void startWithTls(MinecraftApi minecraftApi, SslContext sslContext) {
        requireApi();
    }

    public void stop(boolean closeNioEventLoopGroup) throws InterruptedException {
        connections.clear();
    }

    public void tick() {}

    public int getPort() {
        String port = WorldEnvNatives.setting("management_port");
        return port == null ? hostAndPort.getPort() : Integer.parseInt(port);
    }

    public boolean usesTls() {
        return Boolean.parseBoolean(WorldEnvNatives.setting("management_tls"));
    }

    void forEachConnection(Consumer<Connection> action) {
        connections.forEach(action);
    }

    private static void requireApi() {
        if (!Boolean.parseBoolean(WorldEnvNatives.setting("management_enabled"))) {
            throw new IllegalStateException("The management server is turned off in config/server.toml ([network.management])");
        }
    }
}
