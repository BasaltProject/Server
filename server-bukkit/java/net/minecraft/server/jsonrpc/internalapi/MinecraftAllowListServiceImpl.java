// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.MinecraftServer;
import java.util.Collection;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.UserWhiteListEntry;
public class MinecraftAllowListServiceImpl implements MinecraftAllowListService {
    public MinecraftAllowListServiceImpl(NotificationManager notificationManager, JsonRpcLogger jsonrpcLogger) {
        
    }
    public Collection<UserWhiteListEntry> getEntries() {
        return MinecraftServer.getServer().getPlayerList().getWhiteList().getEntries();
    }
    public boolean add(UserWhiteListEntry infos, ClientInfo clientInfo) {
        return MinecraftServer.getServer().getPlayerList().getWhiteList().add(infos);
    }
    public void clear(ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getWhiteList().clear();
    }
    public void remove(NameAndId nameAndId, ClientInfo clientInfo) {
        MinecraftServer.getServer().getPlayerList().getWhiteList().remove(nameAndId);
    }
    public void kickUnlistedPlayers(ClientInfo clientInfo) {
        net.minecraft.server.players.PlayerList list = MinecraftServer.getServer().getPlayerList();
        for (ServerPlayer player : new java.util.ArrayList<>(list.getPlayers())) {
            if (!list.isWhiteListed(player.nameAndId())) {
                player.connection.disconnect(net.minecraft.network.chat.Component.translatable("multiplayer.disconnect.not_whitelisted"));
            }
        }
    }
}
