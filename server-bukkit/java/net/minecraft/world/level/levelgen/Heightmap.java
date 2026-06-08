package net.minecraft.world.level.levelgen;

import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.chunk.ChunkAccess;
import org.basaltmc.bukkit.Unsupported;
public class Heightmap {
    protected Heightmap() {
    }
    public long[] getRawData() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.Heightmap.getRawData");
    }
    public void setRawData(ChunkAccess chunk, Heightmap.Types type, long[] data) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.Heightmap.setRawData");
    }

    public enum Types implements StringRepresentable {
        WORLD_SURFACE_WG, WORLD_SURFACE, OCEAN_FLOOR_WG, OCEAN_FLOOR, MOTION_BLOCKING, MOTION_BLOCKING_NO_LEAVES;
    }
}
