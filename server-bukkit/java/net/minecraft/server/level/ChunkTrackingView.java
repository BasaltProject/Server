// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.function.Consumer;
import net.minecraft.world.level.ChunkPos;

public interface ChunkTrackingView {
    ChunkTrackingView EMPTY = new ChunkTrackingView() {
        public boolean contains(int chunkX, int chunkZ, boolean includeNeighbors) {
            return false;
        }

        public void forEach(Consumer<ChunkPos> consumer) {}
    };

    static ChunkTrackingView of(ChunkPos center, int radius) {
        return new Positioned(center, radius);
    }

    static void difference(ChunkTrackingView from, ChunkTrackingView to, Consumer<ChunkPos> onEnter, Consumer<ChunkPos> onLeave) {
        if (from.equals(to)) {
            return;
        }
        if (from instanceof Positioned before && to instanceof Positioned after && before.squareIntersects(after)) {
            int x0 = Math.min(before.minX(), after.minX());
            int z0 = Math.min(before.minZ(), after.minZ());
            int x1 = Math.max(before.maxX(), after.maxX());
            int z1 = Math.max(before.maxZ(), after.maxZ());
            for (int x = x0; x <= x1; x++) {
                for (int z = z0; z <= z1; z++) {
                    boolean was = before.contains(x, z);
                    boolean now = after.contains(x, z);
                    if (was != now) {
                        (now ? onEnter : onLeave).accept(new ChunkPos(x, z));
                    }
                }
            }
            return;
        }
        from.forEach(onLeave);
        to.forEach(onEnter);
    }

    default boolean contains(ChunkPos pos) {
        return contains(pos.x(), pos.z());
    }

    default boolean contains(int x, int z) {
        return contains(x, z, true);
    }

    boolean contains(int chunkX, int chunkZ, boolean includeNeighbors);

    void forEach(Consumer<ChunkPos> consumer);

    default boolean isInViewDistance(int chunkX, int chunkZ) {
        return contains(chunkX, chunkZ, false);
    }

    static boolean isInViewDistance(int centerX, int centerZ, int viewDistance, int chunkX, int chunkZ) {
        return isWithinDistance(centerX, centerZ, viewDistance, chunkX, chunkZ, false);
    }

    static boolean isWithinDistance(int centerX, int centerZ, int viewDistance, int chunkX, int chunkZ, boolean includeNeighbors) {
        int margin = includeNeighbors ? 2 : 1;
        long dx = Math.max(0, Math.abs(chunkX - centerX) - margin);
        long dz = Math.max(0, Math.abs(chunkZ - centerZ) - margin);
        return dx * dx + dz * dz < (long) viewDistance * viewDistance;
    }

    public static record Positioned(ChunkPos center, int viewDistance) implements ChunkTrackingView {
        private int minX() {
            return center.x() - viewDistance - 1;
        }

        private int minZ() {
            return center.z() - viewDistance - 1;
        }

        private int maxX() {
            return center.x() + viewDistance + 1;
        }

        private int maxZ() {
            return center.z() + viewDistance + 1;
        }

        boolean squareIntersects(Positioned other) {
            return minX() <= other.maxX() && maxX() >= other.minX() && minZ() <= other.maxZ() && maxZ() >= other.minZ();
        }

        public boolean contains(int chunkX, int chunkZ, boolean includeNeighbors) {
            return ChunkTrackingView.isWithinDistance(center.x(), center.z(), viewDistance, chunkX, chunkZ, includeNeighbors);
        }

        public void forEach(Consumer<ChunkPos> consumer) {
            for (int x = minX(); x <= maxX(); x++) {
                for (int z = minZ(); z <= maxZ(); z++) {
                    if (contains(x, z)) {
                        consumer.accept(new ChunkPos(x, z));
                    }
                }
            }
        }
    }
}
