// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import net.minecraft.server.dedicated.DedicatedServerProperties;
public interface ServerInterface extends ServerInfo {
    default DedicatedServerProperties getProperties() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.ServerInterface#getProperties");
    }
    default String getServerIp() {
        return org.bukkit.Bukkit.getIp();
    }
    default int getServerPort() {
        return org.bukkit.Bukkit.getPort();
    }
    default String getServerName() {
        return org.bukkit.Bukkit.getName();
    }
    default String[] getPlayerNames() {
        return org.bukkit.Bukkit.getOnlinePlayers().stream().map(org.bukkit.entity.Player::getName).toArray(String[]::new);
    }
    default String getLevelIdName() {
        return "world";
    }
    default String getPluginNames() {
        return "";
    }
    default String runCommand(String command) {
        org.bukkit.Bukkit.dispatchCommand(org.bukkit.Bukkit.getConsoleSender(), command);
        return "";
    }
}
