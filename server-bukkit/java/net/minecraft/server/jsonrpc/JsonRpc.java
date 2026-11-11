// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.common.net.HostAndPort;
import io.netty.handler.ssl.SslContext;
import net.minecraft.server.dedicated.DedicatedServerProperties;
import net.minecraft.server.dedicated.DedicatedServerSettings;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.jsonrpc.security.AuthenticationHandler;
import net.minecraft.server.jsonrpc.security.JsonRpcSslContextProvider;
import net.minecraft.server.jsonrpc.security.SecurityConfig;
import net.minecraft.server.notifications.NotificationManager;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class JsonRpc {
    private static final Logger LOG = LoggerFactory.getLogger(JsonRpc.class);

    public static ManagementServer create(DedicatedServerSettings settings, NotificationManager notificationManager) {
        DedicatedServerProperties props = settings.getProperties();
        if (!props.managementServerEnabled) {
            return null;
        }
        if (!SecurityConfig.isValid(props.managementServerSecret)) {
            throw new IllegalStateException("Invalid management server secret, must be 40 alphanumeric characters");
        }
        HostAndPort address = HostAndPort.fromParts(props.managementServerHost, props.managementServerPort);
        LOG.info("Starting json RPC server on {}", address);
        ManagementServer server = new ManagementServer(address,
            new AuthenticationHandler(new SecurityConfig(props.managementServerSecret), props.managementServerAllowedOrigins));
        MinecraftApi api = MinecraftApi.of(notificationManager);
        notificationManager.registerService(new JsonRpcNotificationService(api, server));
        if (props.managementServerTlsEnabled) {
            server.startWithTls(api, tlsContext(props));
        } else {
            server.startWithoutTls(api);
        }
        server.scheduleHeartbeat(notificationManager, props.statusHeartbeatInterval.get());
        return server;
    }

    private static SslContext tlsContext(DedicatedServerProperties props) {
        try {
            return JsonRpcSslContextProvider.createFrom(props.managementServerTlsKeystore, props.managementServerTlsKeystorePassword);
        } catch (Exception failure) {
            JsonRpcSslContextProvider.printInstructions();
            throw new IllegalStateException("Failed to configure TLS for the server management protocol", failure);
        }
    }
}
