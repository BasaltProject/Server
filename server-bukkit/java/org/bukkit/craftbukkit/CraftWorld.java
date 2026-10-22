package org.bukkit.craftbukkit;

import java.util.concurrent.CompletableFuture;
import net.minecraft.server.level.ServerLevel;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.block.Block;
public class CraftWorld extends CraftRegionAccessor {
    protected CraftWorld() {
    }
    public Block getBlockAt(int x, int y, int z) {
        throw Unsupported.hit("org.bukkit.craftbukkit.CraftWorld.getBlockAt");
    }
    public CompletableFuture getChunkAtAsync(int value, int value2) {
        throw Unsupported.hit("org.bukkit.craftbukkit.CraftWorld.getChunkAtAsync");
    }
    public CompletableFuture getChunkAtAsync(int value, int value2, boolean flag, boolean flag2) {
        throw Unsupported.hit("org.bukkit.craftbukkit.CraftWorld.getChunkAtAsync");
    }
    public ServerLevel getHandle() {
        throw Unsupported.hit("org.bukkit.craftbukkit.CraftWorld.getHandle");
    }
    public String getName() {
        throw Unsupported.hit("org.bukkit.craftbukkit.CraftWorld.getName");
    }
}
