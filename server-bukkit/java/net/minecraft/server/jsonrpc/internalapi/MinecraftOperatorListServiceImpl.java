// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.MinecraftServer;
import java.util.Collection;
import java.util.Optional;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.server.permissions.PermissionLevel;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.ServerOpListEntry;
public class MinecraftOperatorListServiceImpl implements MinecraftOperatorListService {
    public MinecraftOperatorListServiceImpl(NotificationManager notificationManager, JsonRpcLogger jsonrpcLogger) {
        
    }
    public Collection<ServerOpListEntry> getEntries() {
        return MinecraftServer.getServer().getPlayerList().getOps().getEntries();
    }
    public void op(NameAndId nameAndId, Optional<PermissionLevel> permissionLevel, Optional<Boolean> canBypassPlayerLimit, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().op(nameAndId, permissionLevel.map(net.minecraft.server.permissions.LevelBasedPermissionSet::forLevel), canBypassPlayerLimit);
    }
    public void op(NameAndId nameAndId, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().op(nameAndId);
    }
    public void deop(NameAndId nameAndId, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().deop(nameAndId);
    }
    public void clear(ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getOps().clear();
    }
}
