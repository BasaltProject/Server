// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.portal;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.BlockUtil;
import net.minecraft.world.level.border.WorldBorder;
import java.util.Comparator;
import net.minecraft.world.level.block.NetherPortalBlock;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.entity.ai.village.poi.PoiTypes;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.entity.ai.village.poi.PoiRecord;
import net.minecraft.util.Mth;
import net.minecraft.world.level.block.state.BlockState;
public class PortalForcer {
    public static final int TICKET_RADIUS = 3;
    private final ServerLevel level;
    public PortalForcer(ServerLevel level) {
        this.level = level;
    }
    public Optional<BlockPos> findClosestPortalPosition(BlockPos approximateExitPos, boolean toNether, WorldBorder worldBorder) {
        PoiManager pois = level.getPoiManager();
        int radius = toNether ? 16 : 128;
        pois.ensureLoadedAndValid(level, approximateExitPos, radius);
        return pois.getInSquare(type -> type.is(PoiTypes.NETHER_PORTAL), approximateExitPos, radius, PoiManager.Occupancy.ANY)
            .map(PoiRecord::getPos)
            .filter(worldBorder::isWithinBounds)
            .filter(pos -> level.getBlockState(pos).hasProperty(BlockStateProperties.HORIZONTAL_AXIS))
            .min(Comparator.comparingDouble((BlockPos pos) -> pos.distSqr(approximateExitPos)).thenComparingInt(BlockPos::getY));
    }
    public Optional<BlockUtil.FoundRectangle> createPortal(BlockPos origin, Direction.Axis portalAxis) {
        Direction forward = Direction.get(Direction.AxisDirection.POSITIVE, portalAxis);
        WorldBorder border = level.getWorldBorder();
        int ceiling = Math.min(level.getMaxY(), level.getMinY() + level.getLogicalHeight() - 1);
        BlockPos.MutableBlockPos scratch = origin.mutable();
        double fullDistance = -1.0;
        BlockPos fullSpot = null;
        double partialDistance = -1.0;
        BlockPos partialSpot = null;
        for (BlockPos.MutableBlockPos column : BlockPos.spiralAround(origin, 16, Direction.EAST, Direction.SOUTH)) {
            int top = Math.min(ceiling, level.getHeight(Heightmap.Types.MOTION_BLOCKING, column.getX(), column.getZ()));
            if (!border.isWithinBounds(column) || !border.isWithinBounds(column.move(forward, 1))) {
                continue;
            }
            column.move(forward.getOpposite(), 1);
            for (int y = top; y >= level.getMinY(); y--) {
                column.setY(y);
                if (!replaceable(column)) {
                    continue;
                }
                int firstFree = y;
                while (y > level.getMinY() && replaceable(column.move(Direction.DOWN))) {
                    y--;
                }
                if (y + 4 > ceiling || (firstFree - y > 0 && firstFree - y < 3)) {
                    continue;
                }
                column.setY(y);
                if (!fitsFrame(column, scratch, forward, 0)) {
                    continue;
                }
                double distance = origin.distSqr(column);
                if (fitsFrame(column, scratch, forward, -1) && fitsFrame(column, scratch, forward, 1) && (fullDistance == -1.0 || fullDistance > distance)) {
                    fullDistance = distance;
                    fullSpot = column.immutable();
                }
                if (fullDistance == -1.0 && (partialDistance == -1.0 || partialDistance > distance)) {
                    partialDistance = distance;
                    partialSpot = column.immutable();
                }
            }
        }
        if (fullDistance == -1.0 && partialDistance != -1.0) {
            fullSpot = partialSpot;
            fullDistance = partialDistance;
        }
        if (fullDistance == -1.0) {
            int lowest = Math.max(level.getMinY() + 1, 70);
            int highest = ceiling - 9;
            if (highest < lowest) {
                return Optional.empty();
            }
            fullSpot = new BlockPos(origin.getX() - forward.getStepX(), Mth.clamp(origin.getY(), lowest, highest), origin.getZ() - forward.getStepZ()).immutable();
            fullSpot = border.clampToBounds(fullSpot);
            Direction side = forward.getClockWise();
            for (int lane = -1; lane < 2; lane++) {
                for (int depth = 0; depth < 2; depth++) {
                    for (int y = -1; y < 3; y++) {
                        BlockState floorOrAir = y < 0 ? Blocks.OBSIDIAN.defaultBlockState() : Blocks.AIR.defaultBlockState();
                        scratch.setWithOffset(fullSpot, depth * forward.getStepX() + lane * side.getStepX(), y, depth * forward.getStepZ() + lane * side.getStepZ());
                        level.setBlockAndUpdate(scratch, floorOrAir);
                    }
                }
            }
        }
        for (int w = -1; w < 3; w++) {
            for (int h = -1; h < 4; h++) {
                if (w == -1 || w == 2 || h == -1 || h == 3) {
                    scratch.setWithOffset(fullSpot, w * forward.getStepX(), h, w * forward.getStepZ());
                    level.setBlockAndUpdate(scratch, Blocks.OBSIDIAN.defaultBlockState());
                }
            }
        }
        BlockState portal = Blocks.NETHER_PORTAL.defaultBlockState().setValue(NetherPortalBlock.AXIS, portalAxis);
        for (int w = 0; w < 2; w++) {
            for (int h = 0; h < 3; h++) {
                scratch.setWithOffset(fullSpot, w * forward.getStepX(), h, w * forward.getStepZ());
                level.setBlock(scratch, portal, 18);
            }
        }
        return Optional.of(new BlockUtil.FoundRectangle(fullSpot.immutable(), 2, 3));
    }
    private boolean replaceable(BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        return state.canBeReplaced() && state.getFluidState().isEmpty();
    }
    private boolean fitsFrame(BlockPos origin, BlockPos.MutableBlockPos scratch, Direction forward, int lane) {
        Direction side = forward.getClockWise();
        for (int w = -1; w < 3; w++) {
            for (int h = -1; h < 4; h++) {
                scratch.setWithOffset(origin, forward.getStepX() * w + side.getStepX() * lane, h, forward.getStepZ() * w + side.getStepZ() * lane);
                boolean ok = h < 0 ? level.getBlockState(scratch).isSolid() : replaceable(scratch);
                if (!ok) {
                    return false;
                }
            }
        }
        return true;
    }
}
