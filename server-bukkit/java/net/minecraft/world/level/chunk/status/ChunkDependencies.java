// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.status;

import com.google.common.collect.ImmutableList;

public final class ChunkDependencies {
    private final ImmutableList<ChunkStatus> byRadius;
    private final int[] radiusByStatus;

    public ChunkDependencies(ImmutableList<ChunkStatus> dependencyByRadius) {
        this.byRadius = dependencyByRadius;
        this.radiusByStatus = new int[dependencyByRadius.isEmpty() ? 0 : dependencyByRadius.getFirst().getIndex() + 1];
        for (int radius = 0; radius < dependencyByRadius.size(); radius++) {
            for (int index = 0; index <= dependencyByRadius.get(radius).getIndex(); index++) {
                radiusByStatus[index] = radius;
            }
        }
    }

    public ImmutableList<ChunkStatus> asList() {
        return byRadius;
    }

    public int size() {
        return byRadius.size();
    }

    public int getRadiusOf(ChunkStatus status) {
        if (status.getIndex() >= radiusByStatus.length) {
            throw new IllegalArgumentException("Requesting a ChunkStatus(" + status + ") outside of dependency range(" + byRadius + ")");
        }
        return radiusByStatus[status.getIndex()];
    }

    public int getRadius() {
        return Math.max(0, byRadius.size() - 1);
    }

    public ChunkStatus get(int distance) {
        return byRadius.get(distance);
    }

    public String toString() {
        return byRadius.toString();
    }
}
