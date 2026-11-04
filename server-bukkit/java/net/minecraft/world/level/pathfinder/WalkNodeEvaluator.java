// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import java.util.EnumSet;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.PathNavigationRegion;
import net.minecraft.world.level.block.DoorBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.material.Fluids;
public class WalkNodeEvaluator extends NodeEvaluator {
    public static final double SPACE_BETWEEN_WALL_POSTS = 0.5;
    private final it.unimi.dsi.fastutil.longs.Long2ObjectMap<PathType> pathTypes = new it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap<>();
    public WalkNodeEvaluator() {
    }
    public void prepare(PathNavigationRegion level, Mob entity) {
        super.prepare(level, entity);
        pathTypes.clear();
    }
    public void done() {
        pathTypes.clear();
        super.done();
    }
    public Node getStart() {
        BlockPos pos = mob.blockPosition();
        int y = Mth.floor(mob.getY() + 0.5);
        Node start = getStartNode(new BlockPos(pos.getX(), y, pos.getZ()));
        return start;
    }
    protected Node getStartNode(BlockPos pos) {
        PathType type = getPathTypeOfMob(currentContext, pos.getX(), pos.getY(), pos.getZ(), mob);
        float malus = mob.getPathfindingMalus(type);
        if (malus < 0.0F && !canStartAt(pos)) {
            return null;
        }
        Node node = getNode(pos);
        node.type = type;
        node.costMalus = Math.max(node.costMalus, malus);
        return node;
    }
    protected boolean canStartAt(BlockPos pos) {
        return mob.getPathfindingMalus(getPathTypeFromState(currentContext.level(), pos)) >= 0.0F;
    }
    public Target getTarget(double x, double y, double z) {
        return getTargetNodeAt(x, y, z);
    }
    public int getNeighbors(Node[] neighbors, Node pos) {
        int count = 0;
        PathType above = getCachedPathType(pos.x, pos.y + 1, pos.z);
        PathType here = getCachedPathType(pos.x, pos.y, pos.z);
        int jump = mob.getPathfindingMalus(above) >= 0.0F && here != PathType.STICKY_HONEY ? Mth.floor(Math.max(1.0F, mob.maxUpStep())) : 0;
        double floor = getFloorLevel(new BlockPos(pos.x, pos.y, pos.z));
        Node[] cardinal = new Node[4];
        Direction[] dirs = {Direction.SOUTH, Direction.WEST, Direction.EAST, Direction.NORTH};
        for (int i = 0; i < 4; i++) {
            Node found = findAcceptedNode(pos.x + dirs[i].getStepX(), pos.y, pos.z + dirs[i].getStepZ(), jump, floor, dirs[i], here);
            cardinal[i] = found;
            if (isNeighborValid(found, pos)) {
                neighbors[count++] = found;
            }
        }
        for (int nsIndex : new int[] {0, 3}) {
            for (int ewIndex : new int[] {1, 2}) {
                Node ns = cardinal[nsIndex];
                Node ew = cardinal[ewIndex];
                if (ns == null || ew == null) {
                    continue;
                }
                Node diagonal = findAcceptedNode(ew.x, pos.y, ns.z, jump, floor, null, here);
                if (isDiagonalValid(pos, ew, ns) && isDiagonalValid(diagonal) && isNeighborValid(diagonal, pos)) {
                    neighbors[count++] = diagonal;
                }
            }
        }
        return count;
    }
    protected boolean isNeighborValid(Node neighbor, Node current) {
        return neighbor != null && !neighbor.closed && (neighbor.costMalus >= 0.0F || current.costMalus < 0.0F);
    }
    protected boolean isDiagonalValid(Node pos, Node ew, Node ns) {
        return ew.y <= pos.y && ns.y <= pos.y && ew.costMalus >= 0.0F && ns.costMalus >= 0.0F;
    }
    protected boolean isDiagonalValid(Node diagonal) {
        return diagonal != null && !diagonal.closed && diagonal.type != PathType.WALKABLE_DOOR && diagonal.costMalus >= 0.0F;
    }
    protected double getFloorLevel(BlockPos pos) {
        return getFloorLevel(currentContext.level(), pos);
    }
    public static double getFloorLevel(BlockGetter level, BlockPos pos) {
        BlockPos below = pos.below();
        net.minecraft.world.phys.shapes.VoxelShape shape = level.getBlockState(below).getCollisionShape(level, below);
        return below.getY() + (shape.isEmpty() ? 0.0 : shape.max(net.minecraft.core.Direction.Axis.Y));
    }
    protected boolean isAmphibious() {
        return false;
    }
    protected Node findAcceptedNode(int x, int y, int z, int jumpSize, double nodeHeight, Direction travelDirection, PathType blockPathTypeCurrent) {
        Node node = null;
        PathType type = getCachedPathType(x, y, z);
        float malus = mob.getPathfindingMalus(type);
        if (malus >= 0.0F) {
            node = getNode(x, y, z);
            node.type = type;
            node.costMalus = Math.max(node.costMalus, malus);
            if (type == PathType.WALKABLE) {
                return node;
            }
        }
        if (type == PathType.WALKABLE || (isAmphibious() && type == PathType.WATER)) {
            return node;
        }
        if ((node == null || node.costMalus < 0.0F) && jumpSize > 0 && type != PathType.FENCE && type != PathType.UNPASSABLE_RAIL
            && type != PathType.TRAPDOOR && type != PathType.POWDER_SNOW) {
            Node up = findAcceptedNode(x, y + 1, z, jumpSize - 1, nodeHeight, travelDirection, blockPathTypeCurrent);
            if (up != null) {
                node = up;
            }
        }
        if (type == PathType.OPEN) {
            int fall = 0;
            int base = y;
            while (type == PathType.OPEN) {
                y--;
                if (y < mob.level().getMinY()) {
                    Node blocked = getNode(x, base, z);
                    blocked.type = PathType.BLOCKED;
                    blocked.costMalus = -1.0F;
                    return blocked;
                }
                if (fall++ >= mob.getMaxFallDistance()) {
                    Node blocked = getNode(x, y, z);
                    blocked.type = PathType.BLOCKED;
                    blocked.costMalus = -1.0F;
                    return blocked;
                }
                type = getCachedPathType(x, y, z);
                malus = mob.getPathfindingMalus(type);
                if (type != PathType.OPEN && malus >= 0.0F) {
                    node = getNode(x, y, z);
                    node.type = type;
                    node.costMalus = Math.max(node.costMalus, malus);
                    break;
                }
                if (malus < 0.0F) {
                    Node blocked = getNode(x, y, z);
                    blocked.type = PathType.BLOCKED;
                    blocked.costMalus = -1.0F;
                    return blocked;
                }
            }
        }
        return node;
    }
    protected PathType getCachedPathType(int x, int y, int z) {
        long key = BlockPos.asLong(x, y, z);
        PathType cached = pathTypes.get(key);
        if (cached == null) {
            cached = getPathTypeOfMob(currentContext, x, y, z, mob);
            pathTypes.put(key, cached);
        }
        return cached;
    }
    public PathType getPathTypeOfMob(PathfindingContext context, int x, int y, int z, Mob mob) {
        Set<PathType> within = getPathTypeWithinMobBB(context, x, y, z);
        if (within.contains(PathType.BLOCKED)) {
            return PathType.BLOCKED;
        }
        PathType worst = PathType.BLOCKED;
        for (PathType type : within) {
            if (mob.getPathfindingMalus(type) < 0.0F) {
                return type;
            }
            if (mob.getPathfindingMalus(type) >= mob.getPathfindingMalus(worst)) {
                worst = type;
            }
        }
        return within.isEmpty() ? PathType.OPEN : worst;
    }
    public Set<PathType> getPathTypeWithinMobBB(PathfindingContext context, int x, int y, int z) {
        EnumSet<PathType> types = EnumSet.noneOf(PathType.class);
        for (int dx = 0; dx < entityWidth; dx++) {
            for (int dy = 0; dy < entityHeight; dy++) {
                for (int dz = 0; dz < entityDepth; dz++) {
                    PathType type = getPathType(context, x + dx, y + dy, z + dz);
                    BlockPos mobPos = context.mobPosition();
                    if (type == PathType.DOOR_WOOD_CLOSED && canOpenDoors && canPassDoors) {
                        type = PathType.WALKABLE_DOOR;
                    }
                    if (type == PathType.DOOR_OPEN && !canPassDoors) {
                        type = PathType.BLOCKED;
                    }
                    if (type == PathType.RAIL && getPathTypeStatic(context, new BlockPos.MutableBlockPos().set(mobPos)) != PathType.RAIL
                        && getPathTypeStatic(context, new BlockPos.MutableBlockPos().set(mobPos.getX(), mobPos.getY() - 1, mobPos.getZ())) != PathType.RAIL) {
                        type = PathType.UNPASSABLE_RAIL;
                    }
                    types.add(type);
                }
            }
        }
        return types;
    }
    public PathType getPathType(PathfindingContext context, int x, int y, int z) {
        return getPathTypeStatic(context, new BlockPos.MutableBlockPos(x, y, z));
    }
    public static PathType getPathTypeStatic(Mob mob, BlockPos pos) {
        return getPathTypeStatic(new PathfindingContext(mob.level(), mob), new BlockPos.MutableBlockPos().set(pos));
    }
    public static PathType getPathTypeStatic(PathfindingContext context, BlockPos.MutableBlockPos pos) {
        int x = pos.getX();
        int y = pos.getY();
        int z = pos.getZ();
        PathType type = context.getPathTypeFromState(x, y, z);
        if (type == PathType.OPEN && y >= context.level().getMinY() + 1) {
            PathType below = context.getPathTypeFromState(x, y - 1, z);
            type = below != PathType.WALKABLE && below != PathType.OPEN && below != PathType.WATER && below != PathType.LAVA
                ? PathType.WALKABLE : PathType.OPEN;
            if (below == PathType.DAMAGING) {
                type = PathType.DAMAGING;
            } else if (below == PathType.FIRE) {
                type = PathType.FIRE;
            } else if (below == PathType.STICKY_HONEY) {
                type = PathType.STICKY_HONEY;
            } else if (below == PathType.COCOA) {
                type = PathType.COCOA;
            } else if (below == PathType.WALKABLE_DOOR) {
                type = PathType.WALKABLE_DOOR;
            }
        }
        if (type == PathType.WALKABLE) {
            type = checkNeighbourBlocks(context, x, y, z, type);
        }
        return type;
    }
    public static PathType checkNeighbourBlocks(PathfindingContext context, int x, int y, int z, PathType blockPathType) {
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dz == 0) {
                        continue;
                    }
                    PathType neighbour = context.getPathTypeFromState(x + dx, y + dy, z + dz);
                    if (neighbour == PathType.DAMAGING) {
                        return PathType.DAMAGING_IN_NEIGHBOR;
                    }
                    if (neighbour == PathType.FIRE || neighbour == PathType.LAVA) {
                        return PathType.FIRE_IN_NEIGHBOR;
                    }
                    if (neighbour == PathType.WATER) {
                        return PathType.WATER_BORDER;
                    }
                }
            }
        }
        return blockPathType;
    }
    protected static PathType getPathTypeFromState(BlockGetter level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        if (state.isAir()) {
            return PathType.OPEN;
        }
        String path = state.getBlock().blockKey().getPath();
        if (state.getFluidState().getType() == Fluids.LAVA || state.getFluidState().getType() == Fluids.FLOWING_LAVA) {
            return PathType.LAVA;
        }
        if (state.getFluidState().getType() == Fluids.WATER || state.getFluidState().getType() == Fluids.FLOWING_WATER) {
            return PathType.WATER;
        }
        if (path.equals("fire") || path.equals("soul_fire") || path.equals("magma_block") || path.endsWith("campfire")) {
            return PathType.FIRE;
        }
        if (path.equals("cactus") || path.equals("sweet_berry_bush") || path.equals("wither_rose")) {
            return PathType.DAMAGING;
        }
        if (path.equals("honey_block")) {
            return PathType.STICKY_HONEY;
        }
        if (path.equals("cocoa")) {
            return PathType.COCOA;
        }
        if (path.equals("powder_snow")) {
            return PathType.POWDER_SNOW;
        }
        if (path.endsWith("_leaves")) {
            return PathType.LEAVES;
        }
        if (path.endsWith("_trapdoor")) {
            return state.getValue(BlockStateProperties.OPEN) ? PathType.OPEN : PathType.TRAPDOOR;
        }
        if (state.getBlock() instanceof DoorBlock) {
            if (state.getValue(BlockStateProperties.OPEN)) {
                return PathType.DOOR_OPEN;
            }
            return path.equals("iron_door") ? PathType.DOOR_IRON_CLOSED : PathType.DOOR_WOOD_CLOSED;
        }
        if (path.endsWith("rail")) {
            return PathType.RAIL;
        }
        if (path.endsWith("_fence") || path.endsWith("_wall") || (path.endsWith("_fence_gate") && !state.getValue(BlockStateProperties.OPEN))) {
            return PathType.FENCE;
        }
        return state.getCollisionShape(level, pos).isEmpty() ? PathType.OPEN : PathType.BLOCKED;
    }
}
