// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dedicated;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.server.RegistryLayer;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.PlayerList;
import net.minecraft.world.level.storage.PlayerDataStorage;
public class DedicatedPlayerList extends PlayerList {
    public DedicatedPlayerList(DedicatedServer server, LayeredRegistryAccess<RegistryLayer> registries, PlayerDataStorage playerDataStorage) {
        super(server, registries, playerDataStorage, server.notificationManager());
    }
    public void reloadWhiteList() {
        super.reloadWhiteList();
    }
    public boolean isWhiteListed(NameAndId nameAndId) {
        return !isUsingWhitelist() || isOp(nameAndId) || super.isWhiteListed(nameAndId);
    }
    public DedicatedServer getServer() {
        return (DedicatedServer) super.getServer();
    }
    public boolean canBypassPlayerLimit(NameAndId nameAndId) {
        return getOps().canBypassPlayerLimit(nameAndId);
    }
}
