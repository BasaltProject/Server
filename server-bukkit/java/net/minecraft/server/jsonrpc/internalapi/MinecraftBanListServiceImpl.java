// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.MinecraftServer;
import java.util.Collection;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.server.players.IpBanListEntry;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.UserBanListEntry;
public class MinecraftBanListServiceImpl implements MinecraftBanListService {
    public MinecraftBanListServiceImpl(NotificationManager notificationManager, JsonRpcLogger jsonrpcLogger) {
        
    }
    public void addUserBan(UserBanListEntry ban, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getBans().add(ban);
    }
    public void removeUserBan(NameAndId nameAndId, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getBans().remove(nameAndId);
    }
    public void clearUserBans(ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getBans().clear();
    }
    public Collection<UserBanListEntry> getUserBanEntries() {
        return MinecraftServer.getServer().getPlayerList().getBans().getEntries();
    }
    public Collection<IpBanListEntry> getIpBanEntries() {
        return MinecraftServer.getServer().getPlayerList().getIpBans().getEntries();
    }
    public void addIpBan(IpBanListEntry ipBanEntry, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getIpBans().add(ipBanEntry);
    }
    public void clearIpBans(ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getIpBans().clear();
    }
    public void removeIpBan(String ip, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getIpBans().remove(ip);
    }
}
