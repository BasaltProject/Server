// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.chunk.ChunkGenerator;

public class WorldGenerationContext {
    private final int minY;
    private final int height;
    private final int seaLevel;

    public static WorldGenerationContext of(LevelAccessor level) {
        return new WorldGenerationContext(level);
    }

    public WorldGenerationContext(ChunkGenerator generator, LevelHeightAccessor heightAccessor) {
        this.minY = Math.max(heightAccessor.getMinY(), generator.getMinY());
        this.height = Math.min(heightAccessor.getHeight(), generator.getGenDepth());
        this.seaLevel = generator.getSeaLevel();
    }

    private WorldGenerationContext(LevelAccessor level) {
        this.minY = level.getMinY();
        this.height = level.getHeight();
        this.seaLevel = level.getSeaLevel();
    }

    public int getMinGenY() {
        return minY;
    }

    public int getGenDepth() {
        return height;
    }

    public int seaLevel() {
        return seaLevel;
    }
}
