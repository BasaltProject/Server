// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.portal;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.BlockUtil;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
import org.apache.commons.lang3.mutable.MutableInt;
import net.minecraft.tags.BlockTags;
import net.minecraft.util.Mth;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.level.block.NetherPortalBlock;
import net.minecraft.world.level.block.Blocks;
public class PortalShape {
    private static final int kMinWidth = 2;
    public static final int MAX_WIDTH = 21;
    private static final int kMinHeight = 3;
    public static final int MAX_HEIGHT = 21;
    public static final Predicate<BlockState> FRAME = state -> state.is(BlockTags.NETHER_PORTAL_FRAME);
    private final Direction.Axis axis;
    private final Direction rightDir;
    private final int portalBlocks;
    private final BlockPos bottomLeft;
    private final int height;
    private final int width;
    private PortalShape(Direction.Axis axis, int portalBlockCount, Direction rightDir, BlockPos bottomLeft, int width, int height) {
        this.axis = axis;
        this.portalBlocks = portalBlockCount;
        this.rightDir = rightDir;
        this.bottomLeft = bottomLeft;
        this.width = width;
        this.height = height;
    }
    public static Optional<PortalShape> findEmptyPortalShape(LevelAccessor level, BlockPos pos, Direction.Axis preferredAxis) {
        return findPortalShape(level, pos, shape -> shape.isValid() && shape.portalBlocks == 0, preferredAxis);
    }
    public static Optional<PortalShape> findPortalShape(LevelAccessor level, BlockPos pos, Predicate<PortalShape> isValid, Direction.Axis preferredAxis) {
        Optional<PortalShape> first = Optional.of(findAnyShape(level, pos, preferredAxis)).filter(isValid);
        if (first.isPresent()) {
            return first;
        }
        Direction.Axis other = preferredAxis == Direction.Axis.X ? Direction.Axis.Z : Direction.Axis.X;
        return Optional.of(findAnyShape(level, pos, other)).filter(isValid);
    }
    public static PortalShape findAnyShape(BlockGetter level, BlockPos pos, Direction.Axis axis) {
        Direction right = axis == Direction.Axis.X ? Direction.WEST : Direction.SOUTH;
        BlockPos corner = findCorner(level, right, pos);
        if (corner == null) {
            return new PortalShape(axis, 0, right, pos, 0, 0);
        }
        int span = measureWidth(level, corner, right);
        if (span == 0) {
            return new PortalShape(axis, 0, right, corner, 0, 0);
        }
        MutableInt filled = new MutableInt();
        int rise = measureHeight(level, corner, right, span, filled);
        return new PortalShape(axis, filled.intValue(), right, corner, span, rise);
    }
    private static BlockPos findCorner(BlockGetter level, Direction right, BlockPos start) {
        BlockPos pos = start;
        int floor = Math.max(level.getMinY(), pos.getY() - MAX_HEIGHT);
        while (pos.getY() > floor && isFree(level.getBlockState(pos.below()))) {
            pos = pos.below();
        }
        Direction left = right.getOpposite();
        int edge = edgeDistance(level, pos, left) - 1;
        return edge < 0 ? null : pos.relative(left, edge);
    }
    private static int measureWidth(BlockGetter level, BlockPos corner, Direction right) {
        int span = edgeDistance(level, corner, right);
        return span >= kMinWidth && span <= MAX_WIDTH ? span : 0;
    }
    private static int edgeDistance(BlockGetter level, BlockPos pos, Direction direction) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int step = 0; step <= MAX_WIDTH; step++) {
            cursor.set(pos).move(direction, step);
            BlockState here = level.getBlockState(cursor);
            if (!isFree(here)) {
                return FRAME.test(here) ? step : 0;
            }
            if (!FRAME.test(level.getBlockState(cursor.move(Direction.DOWN)))) {
                return 0;
            }
        }
        return 0;
    }
    private static int measureHeight(BlockGetter level, BlockPos corner, Direction right, int span, MutableInt filled) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        int rise = climb(level, corner, right, cursor, span, filled);
        return rise >= kMinHeight && rise <= MAX_HEIGHT && topIsFramed(level, corner, right, cursor, span, rise) ? rise : 0;
    }
    private static boolean topIsFramed(BlockGetter level, BlockPos corner, Direction right, BlockPos.MutableBlockPos cursor, int span, int rise) {
        for (int i = 0; i < span; i++) {
            cursor.set(corner).move(Direction.UP, rise).move(right, i);
            if (!FRAME.test(level.getBlockState(cursor))) {
                return false;
            }
        }
        return true;
    }
    private static int climb(BlockGetter level, BlockPos corner, Direction right, BlockPos.MutableBlockPos cursor, int span, MutableInt filled) {
        for (int row = 0; row < MAX_HEIGHT; row++) {
            cursor.set(corner).move(Direction.UP, row).move(right, -1);
            if (!FRAME.test(level.getBlockState(cursor))) {
                return row;
            }
            cursor.set(corner).move(Direction.UP, row).move(right, span);
            if (!FRAME.test(level.getBlockState(cursor))) {
                return row;
            }
            for (int i = 0; i < span; i++) {
                cursor.set(corner).move(Direction.UP, row).move(right, i);
                BlockState state = level.getBlockState(cursor);
                if (!isFree(state)) {
                    return row;
                }
                if (state.is(Blocks.NETHER_PORTAL)) {
                    filled.increment();
                }
            }
        }
        return MAX_HEIGHT;
    }
    private static boolean isFree(BlockState state) {
        return state.isAir() || state.is(BlockTags.FIRE) || state.is(Blocks.NETHER_PORTAL);
    }
    public boolean isValid() {
        return width >= kMinWidth && width <= MAX_WIDTH && height >= kMinHeight && height <= MAX_HEIGHT;
    }
    public void createPortalBlocks(LevelAccessor level) {
        BlockState portal = Blocks.NETHER_PORTAL.defaultBlockState().setValue(NetherPortalBlock.AXIS, axis);
        BlockPos far = bottomLeft.relative(Direction.UP, height - 1).relative(rightDir, width - 1);
        BlockPos.betweenClosed(bottomLeft, far).forEach(pos -> level.setBlock(pos, portal, 18));
    }
    public boolean isComplete() {
        return isValid() && portalBlocks == width * height;
    }
    public static Vec3 getRelativePosition(BlockUtil.FoundRectangle largestRectangleAround, Direction.Axis axis, Vec3 position, EntityDimensions dimensions) {
        double slackX = largestRectangleAround.axis1Size - dimensions.width();
        double slackY = largestRectangleAround.axis2Size - dimensions.height();
        BlockPos min = largestRectangleAround.minCorner;
        double right = 0.5;
        if (slackX > 0.0) {
            double start = min.get(axis) + dimensions.width() / 2.0;
            right = Mth.clamp(Mth.inverseLerp(position.get(axis) - start, 0.0, slackX), 0.0, 1.0);
        }
        double up = 0.0;
        if (slackY > 0.0) {
            up = Mth.clamp(Mth.inverseLerp(position.get(Direction.Axis.Y) - min.get(Direction.Axis.Y), 0.0, slackY), 0.0, 1.0);
        }
        Direction.Axis depth = axis == Direction.Axis.X ? Direction.Axis.Z : Direction.Axis.X;
        return new Vec3(right, up, position.get(depth) - (min.get(depth) + 0.5));
    }
    public static Vec3 findCollisionFreePosition(Vec3 bottomCenter, ServerLevel serverLevel, Entity entity, EntityDimensions dimensions) {
        if (dimensions.width() > 4.0F || dimensions.height() > 4.0F) {
            return bottomCenter;
        }
        double half = dimensions.height() / 2.0;
        Vec3 center = bottomCenter.add(0.0, half, 0.0);
        AABB strip = AABB.ofSize(center, dimensions.width(), 0.0, dimensions.width()).expandTowards(0.0, 1.0, 0.0).inflate(1.0E-6);
        VoxelShape allowed = Shapes.create(strip);
        return serverLevel.findFreePosition(entity, allowed, center, dimensions.width(), dimensions.height(), dimensions.width())
            .map(found -> found.subtract(0.0, half, 0.0)).orElse(bottomCenter);
    }
}
