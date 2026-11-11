// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.MinecraftServer;
import java.util.Collection;
import net.minecraft.network.chat.Component;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.notifications.NotificationManager;
public class MinecraftServerStateServiceImpl implements MinecraftServerStateService {
    public MinecraftServerStateServiceImpl(NotificationManager notificationManager, JsonRpcLogger jsonrpcLogger) {
        
    }
    public boolean isReady() {
        return MinecraftServer.getServer().isReady();
    }
    public boolean saveEverything(boolean suppressLogs, boolean flush, boolean force, ClientInfo clientInfo) {
        return MinecraftServer.getServer().saveEverything(suppressLogs, flush, force);
    }
    public void halt(boolean waitForShutdown, ClientInfo clientInfo) {
        MinecraftServer.getServer().halt(waitForShutdown);
    }
    public void sendSystemMessage(Component message, ClientInfo clientInfo) {
        MinecraftServer.getServer().sendSystemMessage(message);
    }
    public void sendSystemMessage(Component message, boolean overlay, Collection<ServerPlayer> players, ClientInfo clientInfo) {
        for (ServerPlayer player : players) {
            player.sendSystemMessage(message, overlay);
        }
    }
    public void broadcastSystemMessage(Component message, boolean overlay, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().broadcastSystemMessage(message, overlay);

    }
}
