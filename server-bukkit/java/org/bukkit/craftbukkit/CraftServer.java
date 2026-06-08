package org.bukkit.craftbukkit;

import java.io.File;
import java.util.Map;
import net.minecraft.server.dedicated.DedicatedServer;
import org.basaltmc.bukkit.BasaltServer;
import org.bukkit.World;

/** The class plugins find behind Bukkit.getServer(): the package name is how they recognise a CraftBukkit-shaped server. */
public final class CraftServer extends BasaltServer {
    private final DedicatedServer console = DedicatedServer.instance();
    private Map<String, World> worlds;

    public CraftServer(File root, File pluginsFolder) {
        super(root, pluginsFolder);
    }

    public DedicatedServer getServer() {
        return console;
    }
}
