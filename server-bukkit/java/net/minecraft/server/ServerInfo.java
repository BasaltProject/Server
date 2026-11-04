// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;
public interface ServerInfo {
    default String getMotd() {
        return org.bukkit.Bukkit.getMotd();
    }
    default String getServerVersion() {
        return org.bukkit.Bukkit.getMinecraftVersion();
    }
    default int getPlayerCount() {
        return org.bukkit.Bukkit.getOnlinePlayers().size();
    }
    default int getMaxPlayers() {
        return org.bukkit.Bukkit.getMaxPlayers();
    }
}
