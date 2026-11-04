// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import it.unimi.dsi.fastutil.longs.Long2ObjectMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.PathNavigationRegion;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluids;
public class SwimNodeEvaluator extends NodeEvaluator {
    private final boolean allowBreaching;
    private final Long2ObjectMap<PathType> pathTypes = new Long2ObjectOpenHashMap<>();
    public SwimNodeEvaluator(boolean allowBreaching) {
        this.allowBreaching = allowBreaching;
    }
    public void prepare(PathNavigationRegion level, Mob entity) {
        super.prepare(level, entity);
        pathTypes.clear();
    }
    public void done() {
        super.done();
        pathTypes.clear();
    }
    public Node getStart() {
        return getNode(Mth.floor(mob.getBoundingBox().minX), Mth.floor(mob.getBoundingBox().minY + 0.5), Mth.floor(mob.getBoundingBox().minZ));
    }
    public Target getTarget(double x, double y, double z) {
        return getTargetNodeAt(x, y + 0.5, z);
    }
    public int getNeighbors(Node[] neighbors, Node pos) {
        int count = 0;
        for (Direction direction : Direction.values()) {
            Node node = findAcceptedNode(pos.x + direction.getStepX(), pos.y + direction.getStepY(), pos.z + direction.getStepZ());
            if (node != null && isNodeValid(node)) {
                neighbors[count++] = node;
            }
        }
        return count;
    }
    protected boolean isNodeValid(Node node) {
        return !node.closed;
    }
    protected Node findAcceptedNode(int x, int y, int z) {
        PathType type = getCachedBlockType(x, y, z);
        if ((allowBreaching && type == PathType.BREACH) || type == PathType.WATER) {
            float malus = mob.getPathfindingMalus(type);
            if (malus >= 0.0F) {
                Node node = getNode(x, y, z);
                node.type = type;
                node.costMalus = Math.max(node.costMalus, malus);
                if (currentContext.level().getFluidState(new BlockPos(x, y, z)).isEmpty()) {
                    node.costMalus += 8.0F;
                }
                return node;
            }
        }
        return null;
    }
    protected PathType getCachedBlockType(int x, int y, int z) {
        long key = BlockPos.asLong(x, y, z);
        PathType cached = pathTypes.get(key);
        if (cached == null) {
            cached = getPathType(currentContext, x, y, z);
            pathTypes.put(key, cached);
        }
        return cached;
    }
    public PathType getPathType(PathfindingContext context, int x, int y, int z) {
        BlockPos pos = new BlockPos(x, y, z);
        BlockState state = context.getBlockState(pos);
        boolean inWater = state.getFluidState().getType() == Fluids.WATER || state.getFluidState().getType() == Fluids.FLOWING_WATER;
        if (!inWater && state.isAir() || (!inWater && state.getCollisionShape(context.level(), pos).isEmpty())) {
            BlockState below = context.getBlockState(pos.below());
            boolean waterBelow = below.getFluidState().getType() == Fluids.WATER;
            return allowBreaching && waterBelow ? PathType.BREACH : PathType.BLOCKED;
        }
        return inWater && state.getCollisionShape(context.level(), pos).isEmpty() ? PathType.WATER : PathType.BLOCKED;
    }
    public PathType getPathTypeOfMob(PathfindingContext context, int x, int y, int z, Mob mob) {
        return getPathType(context, x, y, z);
    }
}
