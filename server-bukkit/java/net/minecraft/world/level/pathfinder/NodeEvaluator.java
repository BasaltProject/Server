// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import it.unimi.dsi.fastutil.ints.Int2ObjectMap;
import it.unimi.dsi.fastutil.ints.Int2ObjectOpenHashMap;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.PathNavigationRegion;
import net.minecraft.world.level.block.state.BlockState;
public abstract class NodeEvaluator {
    protected PathfindingContext currentContext;
    protected Mob mob;
    protected final Int2ObjectMap<Node> nodes = new Int2ObjectOpenHashMap<>();
    protected int entityWidth;
    protected int entityHeight;
    protected int entityDepth;
    protected boolean canPassDoors;
    protected boolean canOpenDoors;
    protected boolean canFloat;
    protected boolean canWalkOverFences;
    public void prepare(PathNavigationRegion level, Mob entity) {
        currentContext = new PathfindingContext(level, entity);
        mob = entity;
        nodes.clear();
        entityWidth = Mth.floor(entity.getBbWidth() + 1.0F);
        entityHeight = Mth.floor(entity.getBbHeight() + 1.0F);
        entityDepth = Mth.floor(entity.getBbWidth() + 1.0F);
    }
    public void done() {
        currentContext = null;
        mob = null;
    }
    protected Node getNode(BlockPos pos) {
        return getNode(pos.getX(), pos.getY(), pos.getZ());
    }
    protected Node getNode(int x, int y, int z) {
        return nodes.computeIfAbsent(Node.createHash(x, y, z), key -> new Node(x, y, z));
    }
    public abstract Node getStart();
    public abstract Target getTarget(double x, double y, double z);
    protected Target getTargetNodeAt(double x, double y, double z) {
        return new Target(getNode(Mth.floor(x), Mth.floor(y), Mth.floor(z)));
    }
    public abstract int getNeighbors(Node[] neighbors, Node pos);
    public abstract PathType getPathTypeOfMob(PathfindingContext context, int x, int y, int z, Mob mob);
    public abstract PathType getPathType(PathfindingContext context, int x, int y, int z);
    public PathType getPathType(Mob mob, BlockPos pos) {
        return getPathType(new PathfindingContext(mob.level(), mob), pos.getX(), pos.getY(), pos.getZ());
    }
    public void setCanPassDoors(boolean canPassDoors) {
        this.canPassDoors = canPassDoors;
    }
    public void setCanOpenDoors(boolean canOpenDoors) {
        this.canOpenDoors = canOpenDoors;
    }
    public void setCanFloat(boolean canFloat) {
        this.canFloat = canFloat;
    }
    public void setCanWalkOverFences(boolean canWalkOverFences) {
        this.canWalkOverFences = canWalkOverFences;
    }
    public boolean canPassDoors() {
        return canPassDoors;
    }
    public boolean canOpenDoors() {
        return canOpenDoors;
    }
    public boolean canFloat() {
        return canFloat;
    }
    public boolean canWalkOverFences() {
        return canWalkOverFences;
    }
    public static boolean isBurningBlock(BlockState blockState) {
        return blockState.is(net.minecraft.world.level.block.Blocks.FIRE) || blockState.is(net.minecraft.world.level.block.Blocks.LAVA)
            || blockState.is(net.minecraft.world.level.block.Blocks.MAGMA_BLOCK) || blockState.is(net.minecraft.world.level.block.Blocks.CAMPFIRE)
            || blockState.is(net.minecraft.world.level.block.Blocks.SOUL_CAMPFIRE) || blockState.is(net.minecraft.world.level.block.Blocks.LAVA_CAULDRON);
    }
}
