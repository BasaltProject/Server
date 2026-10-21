package org.bukkit.craftbukkit;

import java.io.File;
import net.minecraft.server.MinecraftServer;
import org.basaltmc.bukkit.BasaltServer;

/** The class plugins find behind Bukkit.getServer(): the package name is how they recognise a CraftBukkit-shaped server. */
public final class CraftServer extends BasaltServer {
    private final MinecraftServer console = new MinecraftServer();

    public CraftServer(File root, File pluginsFolder) {
        super(root, pluginsFolder);
    }

    public MinecraftServer getServer() {
        return console;
    }
}
