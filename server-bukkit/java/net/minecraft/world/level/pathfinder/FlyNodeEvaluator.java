// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.PathNavigationRegion;
public class FlyNodeEvaluator extends WalkNodeEvaluator {
    private final it.unimi.dsi.fastutil.longs.Long2ObjectMap<PathType> flyTypes = new it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap<>();
    public void prepare(PathNavigationRegion level, Mob entity) {
        super.prepare(level, entity);
        flyTypes.clear();
    }
    public void done() {
        flyTypes.clear();
        super.done();
    }
    public Node getStart() {
        return getStartNode(new BlockPos(Mth.floor(mob.getBoundingBox().minX), Mth.floor(mob.getBoundingBox().minY + 0.5),
            Mth.floor(mob.getBoundingBox().minZ)));
    }
    protected boolean canStartAt(BlockPos pos) {
        return mob.getPathfindingMalus(getPathType(currentContext, pos.getX(), pos.getY(), pos.getZ())) >= 0.0F;
    }
    public Target getTarget(double x, double y, double z) {
        return getTargetNodeAt(x, y, z);
    }
    public int getNeighbors(Node[] neighbors, Node pos) {
        int count = 0;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dy == 0 && dz == 0) {
                        continue;
                    }
                    Node node = findAcceptedNode(pos.x + dx, pos.y + dy, pos.z + dz);
                    if (node != null && !node.closed) {
                        neighbors[count++] = node;
                    }
                }
            }
        }
        return count;
    }
    protected Node findAcceptedNode(int x, int y, int z) {
        PathType type = getCachedPathType(x, y, z);
        float malus = mob.getPathfindingMalus(type);
        if (malus < 0.0F) {
            return null;
        }
        Node node = getNode(x, y, z);
        node.type = type;
        node.costMalus = Math.max(node.costMalus, malus);
        return node;
    }
    protected PathType getCachedPathType(int x, int y, int z) {
        long key = BlockPos.asLong(x, y, z);
        PathType cached = flyTypes.get(key);
        if (cached == null) {
            cached = getPathType(currentContext, x, y, z);
            flyTypes.put(key, cached);
        }
        return cached;
    }
    public PathType getPathType(PathfindingContext context, int x, int y, int z) {
        PathType type = context.getPathTypeFromState(x, y, z);
        return type == PathType.OPEN ? checkNeighbourBlocks(context, x, y, z, type) : type;
    }
}
