package net.minecraft.server.dedicated;

import net.minecraft.core.RegistryAccess;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.ServerInterface;

public class DedicatedServer extends MinecraftServer implements ServerInterface {
    private static final DedicatedServer kInstance = new DedicatedServer();

    private DedicatedServer() {}

    public static DedicatedServer instance() {
        return kInstance;
    }

    public static MinecraftServer getServer() {
        return kInstance;
    }

    @Override
    public RegistryAccess.Frozen registryAccess() {
        return super.registryAccess();
    }
}
