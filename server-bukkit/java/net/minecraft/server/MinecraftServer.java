package net.minecraft.server;

import com.mojang.datafixers.DataFixer;
import net.minecraft.core.RegistryAccess;
import net.minecraft.server.dedicated.DedicatedServer;
import net.minecraft.server.network.ServerConnectionListener;
import org.basaltmc.bukkit.Natives;
import org.basaltmc.bukkit.facade.ServerAccess;
import org.basaltmc.bukkit.facade.TickQueue;

public class MinecraftServer {
    public static volatile int currentTick;
    private final ServerConnectionListener connection = new ServerConnectionListener();
    private DataFixer fixer;

    protected MinecraftServer() {}

    public static MinecraftServer getServer() {
        return DedicatedServer.instance();
    }

    public RegistryAccess.Frozen registryAccess() {
        return ServerAccess.registries();
    }

    public void execute(Runnable task) {
        TickQueue.run(task);
    }

    public boolean isSameThread() {
        return Natives.onPluginThread();
    }

    public synchronized DataFixer getFixerUpper() {
        if (fixer == null) {
            fixer = ServerAccess.dataFixer();
        }
        return fixer;
    }

    public boolean isDebugging() {
        return false;
    }

    public ServerConnectionListener getConnection() {
        return connection;
    }
}
