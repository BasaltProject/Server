// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.pattern;

import com.google.common.cache.CacheBuilder;
import com.google.common.cache.CacheLoader;
import com.google.common.cache.LoadingCache;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelReader;

public class BlockPattern {
    private final Predicate<BlockInWorld>[][][] pattern;
    private final int depth;
    private final int height;
    private final int width;

    public BlockPattern(Predicate<BlockInWorld>[][][] pattern) {
        this.pattern = pattern;
        this.depth = pattern.length;
        this.height = depth > 0 ? pattern[0].length : 0;
        this.width = height > 0 ? pattern[0][0].length : 0;
    }

    public int getDepth() {
        return depth;
    }

    public int getHeight() {
        return height;
    }

    public int getWidth() {
        return width;
    }

    public Predicate<BlockInWorld>[][][] getPattern() {
        return pattern;
    }

    public BlockPatternMatch matches(LevelReader level, BlockPos origin, Direction forwards, Direction up) {
        return matchAt(origin, forwards, up, createLevelCache(level, false));
    }

    private BlockPatternMatch matchAt(BlockPos origin, Direction forwards, Direction up, LoadingCache<BlockPos, BlockInWorld> cache) {
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                for (int z = 0; z < depth; z++) {
                    BlockInWorld block = cache.getUnchecked(translateAndRotate(origin, forwards, up, x, y, z));
                    if (!pattern[z][y][x].test(block)) {
                        return null;
                    }
                }
            }
        }
        return new BlockPatternMatch(origin, forwards, up, cache, width, height, depth);
    }

    public BlockPatternMatch find(LevelReader level, BlockPos origin) {
        LoadingCache<BlockPos, BlockInWorld> cache = createLevelCache(level, false);
        int reach = Math.max(Math.max(width, height), depth) - 1;
        for (BlockPos candidate : BlockPos.betweenClosed(origin, origin.offset(reach, reach, reach))) {
            for (Direction forwards : Direction.values()) {
                for (Direction up : Direction.values()) {
                    if (up == forwards || up == forwards.getOpposite()) {
                        continue;
                    }
                    BlockPatternMatch match = matchAt(candidate, forwards, up, cache);
                    if (match != null) {
                        return match;
                    }
                }
            }
        }
        return null;
    }

    public static LoadingCache<BlockPos, BlockInWorld> createLevelCache(LevelReader level, boolean loadChunks) {
        return CacheBuilder.newBuilder().build(new BlockCacheLoader(level, loadChunks));
    }

    protected static BlockPos translateAndRotate(BlockPos origin, Direction forwardsDirection, Direction upDirection, int right, int down, int forwards) {
        if (forwardsDirection == upDirection || forwardsDirection == upDirection.getOpposite()) {
            throw new IllegalArgumentException("Invalid forwards & up combination");
        }
        int fx = forwardsDirection.getStepX();
        int fy = forwardsDirection.getStepY();
        int fz = forwardsDirection.getStepZ();
        int ux = upDirection.getStepX();
        int uy = upDirection.getStepY();
        int uz = upDirection.getStepZ();
        int rx = fy * uz - fz * uy;
        int ry = fz * ux - fx * uz;
        int rz = fx * uy - fy * ux;
        return origin.offset(ux * -down + rx * right + fx * forwards, uy * -down + ry * right + fy * forwards,
            uz * -down + rz * right + fz * forwards);
    }

    static class BlockCacheLoader extends CacheLoader<BlockPos, BlockInWorld> {
        private final LevelReader level;
        private final boolean loadChunks;

        public BlockCacheLoader(LevelReader level, boolean loadChunks) {
            this.level = level;
            this.loadChunks = loadChunks;
        }

        public BlockInWorld load(BlockPos key) {
            return new BlockInWorld(level, key, loadChunks);
        }
    }

    public static class BlockPatternMatch {
        private final BlockPos frontTopLeft;
        private final Direction forwards;
        private final Direction up;
        private final LoadingCache<BlockPos, BlockInWorld> cache;
        private final int width;
        private final int height;
        private final int depth;

        public BlockPatternMatch(BlockPos frontTopLeft, Direction forwards, Direction up, LoadingCache<BlockPos, BlockInWorld> cache, int width, int height, int depth) {
            this.frontTopLeft = frontTopLeft;
            this.forwards = forwards;
            this.up = up;
            this.cache = cache;
            this.width = width;
            this.height = height;
            this.depth = depth;
        }

        public BlockPos getFrontTopLeft() {
            return frontTopLeft;
        }

        public Direction getForwards() {
            return forwards;
        }

        public Direction getUp() {
            return up;
        }

        public int getWidth() {
            return width;
        }

        public int getHeight() {
            return height;
        }

        public int getDepth() {
            return depth;
        }

        public BlockInWorld getBlock(int right, int down, int forwards) {
            return cache.getUnchecked(BlockPattern.translateAndRotate(frontTopLeft, this.forwards, up, right, down, forwards));
        }

        public String toString() {
            return "BlockPatternMatch{up=" + up + ", forwards=" + forwards + ", frontTopLeft=" + frontTopLeft + "}";
        }
    }
}
