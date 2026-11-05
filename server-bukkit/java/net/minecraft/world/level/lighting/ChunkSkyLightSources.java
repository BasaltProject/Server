// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.chunk.ChunkAccess;
public class ChunkSkyLightSources {
    protected ChunkSkyLightSources() {
        this.minY = 0;
        this.lowest = new int[256];
    }
    public static final int NEGATIVE_INFINITY = Integer.MIN_VALUE;
    private final int minY;
    private final int[] lowest;
    public ChunkSkyLightSources(LevelHeightAccessor level) {
        this.minY = level.getMinY() - 1;
        this.lowest = new int[256];
        java.util.Arrays.fill(lowest, minY);
    }
    public void fillFrom(ChunkAccess chunk) {
        for (int z = 0; z < 16; z++) {
            for (int x = 0; x < 16; x++) {
                lowest[x + z * 16] = scanDown(chunk, x, chunk.getMaxY() + 1, z);
            }
        }
    }
    private int scanDown(BlockGetter level, int x, int fromY, int z) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int y = fromY; y > minY; y--) {
            if (occludes(level.getBlockState(cursor.set(x, y - 1, z)))) {
                return y;
            }
        }
        return minY;
    }
    private static boolean occludes(BlockState below) {
        return below.getLightDampening() != 0;
    }
    public boolean update(BlockGetter level, int x, int y, int z) {
        int index = x + z * 16;
        int current = lowest[index];
        if (y + 1 < current) {
            return false;
        }
        int refreshed = scanDown(level, x, Math.max(y + 1, current), z);
        lowest[index] = refreshed;
        return refreshed != current;
    }
    public int getLowestSourceY(int x, int z) {
        int value = lowest[x + z * 16];
        return value == minY ? Integer.MIN_VALUE : value;
    }
    public int getHighestLowestSourceY() {
        int best = Integer.MIN_VALUE;
        for (int value : lowest) {
            best = Math.max(best, value);
        }
        return best == minY ? Integer.MIN_VALUE : best;
    }
}
