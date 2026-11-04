// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.PathNavigationRegion;
public class AmphibiousNodeEvaluator extends WalkNodeEvaluator {
    private final boolean prefersShallowSwimming;
    private float oldWalkableCost;
    private float oldWaterBorderCost;
    public AmphibiousNodeEvaluator(boolean prefersShallowSwimming) {
        this.prefersShallowSwimming = prefersShallowSwimming;
    }
    public void prepare(PathNavigationRegion level, Mob entity) {
        super.prepare(level, entity);
        entity.setPathfindingMalus(PathType.WATER, 0.0F);
        oldWalkableCost = entity.getPathfindingMalus(PathType.WALKABLE);
        entity.setPathfindingMalus(PathType.WALKABLE, 6.0F);
        oldWaterBorderCost = entity.getPathfindingMalus(PathType.WATER_BORDER);
        entity.setPathfindingMalus(PathType.WATER_BORDER, 4.0F);
    }
    public void done() {
        mob.setPathfindingMalus(PathType.WALKABLE, oldWalkableCost);
        mob.setPathfindingMalus(PathType.WATER_BORDER, oldWaterBorderCost);
        super.done();
    }
    public Node getStart() {
        return super.getStart();
    }
    public Target getTarget(double x, double y, double z) {
        return getTargetNodeAt(x, y + 0.5, z);
    }
    public int getNeighbors(Node[] neighbors, Node pos) {
        int count = super.getNeighbors(neighbors, pos);
        PathType above = getCachedPathType(pos.x, pos.y + 1, pos.z);
        PathType here = getCachedPathType(pos.x, pos.y, pos.z);
        int jump = prefersShallowSwimming && here == PathType.WATER ? 0 : (int) Math.max(1.0F, mob.maxUpStep());
        if (mob.getPathfindingMalus(above) >= 0.0F && here != PathType.STICKY_HONEY) {
            Node up = findAcceptedNode(pos.x, pos.y + 1, pos.z, Math.max(0, jump), 0.0, null, here);
            if (up != null && !up.closed && up.type == PathType.WATER && count < neighbors.length) {
                neighbors[count++] = up;
            }
        }
        Node down = findAcceptedNode(pos.x, pos.y - 1, pos.z, jump, 0.0, null, here);
        if (down != null && !down.closed && down.type == PathType.WATER && count < neighbors.length) {
            neighbors[count++] = down;
        }
        return count;
    }
    protected boolean isAmphibious() {
        return true;
    }
    public PathType getPathType(PathfindingContext context, int x, int y, int z) {
        return super.getPathType(context, x, y, z);
    }
}
