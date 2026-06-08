package net.minecraft.world.level.chunk;

import org.basaltmc.bukkit.Unsupported;
public class DataLayer {
    public DataLayer(byte[] values) {
    }
    public int get(int x, int y, int z) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.DataLayer.get");
    }
    public byte[] getData() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.DataLayer.getData");
    }
    public void set(int x, int y, int z, int val) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.DataLayer.set");
    }
}
