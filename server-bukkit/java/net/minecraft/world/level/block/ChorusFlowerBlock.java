// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.shapes.VoxelShape;
public class ChorusFlowerBlock extends Block {
    private static boolean allNeighborsEmpty(LevelReader level, BlockPos pos, Direction ignore) {
        for (Direction side : Direction.Plane.HORIZONTAL) {
            if (side != ignore && !level.isEmptyBlock(pos.relative(side))) {
                return false;
            }
        }
        return true;
    }
    private static void placePlant(LevelAccessor level, BlockPos at) {
        BlockState plant = Blocks.CHORUS_PLANT.defaultBlockState();
        level.setBlock(at, ChorusPlantBlock.getStateWithConnections(level, at, plant), 2);
    }
    private static void growTreeRecursive(LevelAccessor level, BlockPos origin, RandomSource random, BlockPos start, int spread, int depth) {
        int trunk = random.nextInt(4) + 1 + (depth == 0 ? 1 : 0);
        for (int i = 1; i <= trunk; i++) {
            BlockPos segment = origin.above(i);
            if (!allNeighborsEmpty(level, segment, null)) {
                return;
            }
            placePlant(level, segment);
            placePlant(level, segment.below());
        }
        boolean branched = false;
        if (depth < 4) {
            int branches = random.nextInt(4) + (depth == 0 ? 1 : 0);
            for (int i = 0; i < branches; i++) {
                Direction way = Direction.Plane.HORIZONTAL.getRandomDirection(random);
                BlockPos tip = origin.above(trunk).relative(way);
                boolean inRange = Math.abs(tip.getX() - start.getX()) < spread && Math.abs(tip.getZ() - start.getZ()) < spread;
                if (inRange && level.isEmptyBlock(tip) && level.isEmptyBlock(tip.below())
                    && allNeighborsEmpty(level, tip, way.getOpposite())) {
                    branched = true;
                    placePlant(level, tip);
                    placePlant(level, tip.relative(way.getOpposite()));
                    growTreeRecursive(level, tip, random, start, spread, depth + 1);
                }
            }
        }
        if (!branched) {
            level.setBlock(origin.above(trunk), Blocks.CHORUS_FLOWER.defaultBlockState().setValue(AGE, 5), 2);
        }
    }
    public static final int DEAD_AGE = 5;
    public static final IntegerProperty AGE = net.minecraft.world.level.block.state.properties.BlockStateProperties.AGE_5;
    protected ChorusFlowerBlock(Block plant, BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected boolean isRandomlyTicking(BlockState state) {
        return super.isRandomlyTicking(state);
    }
    public VoxelShape getBlockSupportShape(BlockState state, BlockGetter level, BlockPos pos) {
        return super.getBlockSupportShape(state, level, pos);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public static void generatePlant(LevelAccessor level, BlockPos target, RandomSource random, int maxHorizontalSpread) {
        placePlant(level, target);
        growTreeRecursive(level, target, random, target, maxHorizontalSpread, 0);
    }
    protected void onProjectileHit(Level level, BlockState state, BlockHitResult blockHit, Projectile projectile) {
        super.onProjectileHit(level, state, blockHit, projectile);
    }
}
