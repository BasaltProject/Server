// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.util.RandomSource;
import net.minecraft.core.Direction;
import java.util.List;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
public class ShelfMushroomDecorator extends TreeDecorator {
    private final float placementProbability;
    private static boolean shelfAt(TreeDecorator.Context context, BlockPos pos) {
        return context.checkBlock(pos, state -> state.is(Blocks.SHELF_MUSHROOM));
    }
    private static boolean shelfBeside(TreeDecorator.Context context, BlockPos pos) {
        for (Direction side : Direction.Plane.HORIZONTAL) {
            if (shelfAt(context, pos.relative(side))) {
                return true;
            }
        }
        return false;
    }
    private static void put(TreeDecorator.Context context, BlockPos pos, Direction facing, RandomSource random) {
        context.setBlock(pos, org.basaltmc.bukkit.facade.StateProps.with(org.basaltmc.bukkit.facade.StateProps.with(Blocks.SHELF_MUSHROOM.defaultBlockState(), "age", random.nextInt(2)), "facing", facing.getSerializedName()));
    }
    private static BlockPos spotNextTo(BlockPos log, Direction facing) {
        return log.offset(facing.getStepX(), 0, facing.getStepZ());
    }
    private static void onStanding(TreeDecorator.Context context, List<BlockPos> logs, RandomSource random) {
        Direction first = Direction.Plane.HORIZONTAL.getRandomDirection(random);
        Direction[] sides = {first, first.getClockWise()};
        int baseY = logs.get(0).getY();
        for (BlockPos log : logs) {
            int rise = log.getY() - baseY;
            if (rise < 1 || rise > 4) {
                continue;
            }
            for (Direction facing : sides) {
                if (random.nextFloat() > 0.25F) {
                    continue;
                }
                BlockPos spot = spotNextTo(log, facing);
                if (isBlockReplaceableWithShelfMushroom(context, spot) && !shelfAt(context, spot.below())) {
                    put(context, spot, facing, random);
                    break;
                }
            }
        }
    }
    private static void onFallen(TreeDecorator.Context context, List<BlockPos> logs, RandomSource random) {
        BlockPos head = logs.get(0);
        BlockPos tail = logs.get(logs.size() - 1);
        Direction[] sides = head.getX() != tail.getX() ? new Direction[] {Direction.NORTH, Direction.SOUTH} : new Direction[] {Direction.EAST, Direction.WEST};
        for (BlockPos log : logs) {
            for (Direction facing : sides) {
                if (random.nextFloat() > 0.25F) {
                    continue;
                }
                BlockPos spot = spotNextTo(log, facing);
                if (isBlockReplaceableWithShelfMushroom(context, spot) && !shelfBeside(context, spot) && !shelfBeside(context, log)) {
                    put(context, spot, facing, random);
                }
            }
        }
    }
    public static final MapCodec<ShelfMushroomDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.ShelfMushroomDecorator.class, "CODEC");
    public ShelfMushroomDecorator(float probability) {
        this.placementProbability = probability;
    }
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.SHELF_MUSHROOM;
    }
    public void place(TreeDecorator.Context context) {
        RandomSource random = context.random();
        if (random.nextFloat() >= placementProbability) {
            return;
        }
        List<BlockPos> logs = context.logs();
        if (logs.isEmpty()) {
            return;
        }
        if (logs.get(0).getY() == logs.get(logs.size() - 1).getY()) {
            onFallen(context, logs, random);
        } else {
            onStanding(context, logs, random);
        }
    }
    public static boolean isBlockReplaceableWithShelfMushroom(TreeDecorator.Context context, BlockPos pos) {
        return context.isReplaceable(pos) && !context.isWaterOrWaterNearby(pos);


    }
}
