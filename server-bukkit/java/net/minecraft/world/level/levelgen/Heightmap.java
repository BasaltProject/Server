package net.minecraft.world.level.levelgen;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.SimpleBitStorage;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.LevelChunk;
import org.basaltmc.bukkit.WorldNatives;

/** Heights read from the world on first use; a plugin that writes raw data only replaces this copy. */
public class Heightmap {
    private final LevelChunk chunk;
    private final ServerLevel level;
    private final Types type;
    private SimpleBitStorage data;

    protected Heightmap() {
        this.chunk = null;
        this.level = null;
        this.type = null;
    }

    public Heightmap(LevelChunk chunk, ServerLevel level, Types type) {
        this.chunk = chunk;
        this.level = level;
        this.type = type;
    }

    private synchronized SimpleBitStorage storage() {
        if (data == null) {
            int bits = net.minecraft.util.Mth.ceillog2(level.getHeight() + 1);
            SimpleBitStorage made = new SimpleBitStorage(bits, 256);
            String kind = switch (type) {
                case WORLD_SURFACE_WG, WORLD_SURFACE -> "surface";
                case OCEAN_FLOOR_WG, OCEAN_FLOOR -> "ocean_floor";
                default -> "motion_blocking";
            };
            String world = level.getWorld().getName();
            int minY = level.getMinY();
            for (int z = 0; z < 16; z++) {
                for (int x = 0; x < 16; x++) {
                    int top = WorldNatives.highest(world, (chunk.locX << 4) + x, (chunk.locZ << 4) + z, kind, false);
                    made.set(z << 4 | x, Math.max(0, top + 1 - minY));
                }
            }
            data = made;
        }
        return data;
    }

    public long[] getRawData() {
        return storage().getRaw();
    }

    public void setRawData(ChunkAccess chunk, Heightmap.Types type, long[] raw) {
        int bits = net.minecraft.util.Mth.ceillog2(level.getHeight() + 1);
        synchronized (this) {
            data = new SimpleBitStorage(bits, 256, raw.clone());
        }
    }

    public enum Types implements StringRepresentable {
        WORLD_SURFACE_WG, WORLD_SURFACE, OCEAN_FLOOR_WG, OCEAN_FLOOR, MOTION_BLOCKING, MOTION_BLOCKING_NO_LEAVES;
    }
}
