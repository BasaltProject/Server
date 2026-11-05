// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;

import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;

public class PathTypeCache {
    private static final int kMaxEntries = 4096;
    private final ConcurrentHashMap<Long, PathType> cache = new ConcurrentHashMap<>();

    public PathType getOrCompute(BlockGetter level, BlockPos pos) {
        if (cache.size() >= kMaxEntries) {
            cache.clear();
        }
        return cache.computeIfAbsent(pos.asLong(), key -> WalkNodeEvaluator.getPathTypeFromState(level, pos));
    }

    public void invalidate(BlockPos pos) {
        cache.remove(pos.asLong());
    }
}
