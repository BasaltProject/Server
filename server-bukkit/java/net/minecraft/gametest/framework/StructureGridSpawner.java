// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.world.level.Level;

public class StructureGridSpawner implements GameTestRunner.StructureSpawner {
    private static final int kSpacing = 6;
    private final Function<ResourceKey<Level>, BlockPos> firstTestNorthWestCorner;
    private final int testsPerRow;
    private final boolean clearOnBatch;
    private final Map<ResourceKey<Level>, DimensionGridState> grids = new HashMap<>();

    public StructureGridSpawner(Function<ResourceKey<Level>, BlockPos> firstTestNorthWestCorner, int testsPerRow, boolean clearOnBatch) {
        this.firstTestNorthWestCorner = firstTestNorthWestCorner;
        this.testsPerRow = testsPerRow;
        this.clearOnBatch = clearOnBatch;
    }

    @Override
    public void onBatchStart(MinecraftServer server) {
        if (clearOnBatch) {
            grids.clear();
        }
    }

    @Override
    public Optional<GameTestInfo> spawnStructure(GameTestInfo testInfo) {
        ResourceKey<Level> dimension = testInfo.getLevel().dimension();
        DimensionGridState grid = grids.computeIfAbsent(dimension, key -> new DimensionGridState(firstTestNorthWestCorner.apply(key)));
        testInfo.setTestBlockPos(grid.next(testInfo.getTest().padding()));
        return GameTestRunner.StructureSpawner.IN_PLACE.spawnStructure(testInfo);
    }

    static final class DimensionGridState {
        private final BlockPos start;
        private int column;
        private int rowZ;
        private int rowDepth;
        private int cursorX;

        private DimensionGridState(BlockPos start) {
            this.start = start;
        }

        BlockPos next(int padding) {
            BlockPos pos = start.offset(cursorX, 0, rowZ);
            cursorX += kSpacing + padding * 2;
            rowDepth = Math.max(rowDepth, kSpacing + padding * 2);
            if (++column >= GameTestRunner.DEFAULT_TESTS_PER_ROW) {
                column = 0;
                cursorX = 0;
                rowZ += rowDepth;
                rowDepth = 0;
            }
            return pos;
        }
    }
}
