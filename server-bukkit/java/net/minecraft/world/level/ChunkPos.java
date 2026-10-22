package net.minecraft.world.level;

import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import org.basaltmc.bukkit.Unsupported;
public record ChunkPos(int x, int z) {
    public static ChunkPos containing(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.containing");
    }
    public int getMaxBlockX() {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.getMaxBlockX");
    }
    public int getMaxBlockZ() {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.getMaxBlockZ");
    }
    public int getMinBlockX() {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.getMinBlockX");
    }
    public int getMinBlockZ() {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.getMinBlockZ");
    }
    public static long pack(int x, int z) {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.pack");
    }
    public static Stream rangeClosed(ChunkPos center, ChunkPos radius) {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.rangeClosed");
    }
    public static ChunkPos unpack(long key) {
        throw Unsupported.hit("net.minecraft.world.level.ChunkPos.unpack");
    }
}
