package net.minecraft.server.dedicated;

import net.minecraft.core.RegistryAccess;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.ServerInterface;
import org.basaltmc.bukkit.Unsupported;
public class DedicatedServer extends MinecraftServer implements ServerInterface {
    protected DedicatedServer() {
    }
    public static MinecraftServer getServer() {
        throw Unsupported.hit("net.minecraft.server.dedicated.DedicatedServer.getServer");
    }
    public RegistryAccess.Frozen registryAccess() {
        throw Unsupported.hit("net.minecraft.server.dedicated.DedicatedServer.registryAccess");
    }
}
