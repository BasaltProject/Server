// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.players.PlayerList;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.server.players.NameAndId;
public class MinecraftPlayerListServiceImpl implements MinecraftPlayerListService {
    private static PlayerList list() {
        return MinecraftServer.getServer().getPlayerList();
    }
    public MinecraftPlayerListServiceImpl(NotificationManager notificationManager, JsonRpcLogger jsonRpcLogger) {
        
    }
    public List<ServerPlayer> getPlayers() {
        return list().getPlayers();
    }
    public ServerPlayer getPlayer(UUID uuid) {
        return list().getPlayer(uuid);
    }
    public Optional<NameAndId> fetchUserByName(String name) {
        return MinecraftServer.getServer().services().nameToIdCache().get(name);
    }
    public Optional<NameAndId> fetchUserById(UUID id) {
        return MinecraftServer.getServer().services().nameToIdCache().get(id);
    }
    public Optional<NameAndId> getCachedUserById(UUID id) {
        return MinecraftServer.getServer().services().nameToIdCache().get(id);
    }
    public Optional<ServerPlayer> getPlayer(Optional<UUID> id, Optional<String> name) {
        if (id.isPresent()) {
            return Optional.ofNullable(list().getPlayer(id.get()));
        }
        return name.map(list()::getPlayerByName);
    }
    public List<ServerPlayer> getPlayersWithAddress(String ip) {
        return list().getPlayersWithAddress(ip);
    }
    public void remove(ServerPlayer serverPlayer, ClientInfo clientInfo) {
        serverPlayer.connection.disconnect(net.minecraft.network.chat.Component.translatable("multiplayer.disconnect.kicked"));
    }
    public ServerPlayer getPlayerByName(String name) {
        return list().getPlayerByName(name);
    }
}
