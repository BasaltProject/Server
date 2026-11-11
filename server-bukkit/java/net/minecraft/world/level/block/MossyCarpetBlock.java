// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.EnumMap;
import java.util.function.BooleanSupplier;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.phys.shapes.Shapes;
import java.util.Map;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.WallSide;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class MossyCarpetBlock extends Block implements BonemealableBlock {
    private static Map<Direction, EnumProperty<WallSide>> sideMap() {
        Map<Direction, EnumProperty<WallSide>> sides = new EnumMap<>(Direction.class);
        sides.put(Direction.NORTH, BlockStateProperties.NORTH_WALL);
        sides.put(Direction.EAST, BlockStateProperties.EAST_WALL);
        sides.put(Direction.SOUTH, BlockStateProperties.SOUTH_WALL);
        sides.put(Direction.WEST, BlockStateProperties.WEST_WALL);
        return sides;
    }
    private static boolean hasFaces(BlockState state) {
        if (state.getValue(BASE)) {
            return true;
        }
        for (EnumProperty<WallSide> prop : PROPERTY_BY_DIRECTION.values()) {
            if (state.getValue(prop) != WallSide.NONE) {
                return true;
            }
        }
        return false;
    }
    private static boolean canSupportAtFace(BlockGetter level, BlockPos pos, Direction direction) {
        return direction != Direction.UP && MultifaceBlock.canAttachTo(level, pos, direction);
    }
    private static BlockState getUpdatedState(BlockState state, BlockGetter level, BlockPos pos, boolean createSides) {
        boolean create = createSides || state.getValue(BASE);
        BlockState result = state;
        BlockState above = null;
        BlockState below = null;
        for (Direction dir : Direction.Plane.HORIZONTAL) {
            EnumProperty<WallSide> prop = getPropertyForFace(dir);
            WallSide side = !canSupportAtFace(level, pos, dir) ? WallSide.NONE : create ? WallSide.LOW : result.getValue(prop);
            if (side == WallSide.LOW) {
                above = above == null ? level.getBlockState(pos.above()) : above;
                if (above.is(Blocks.PALE_MOSS_CARPET) && above.getValue(prop) != WallSide.NONE && !above.getValue(BASE)) {
                    side = WallSide.TALL;
                }
                if (!result.getValue(BASE)) {
                    below = below == null ? level.getBlockState(pos.below()) : below;
                    if (below.is(Blocks.PALE_MOSS_CARPET) && below.getValue(prop) == WallSide.NONE) {
                        side = WallSide.NONE;
                    }
                }
            }
            result = result.setValue(prop, side);
        }
        return result;
    }
    private static BlockState topperWithSideChance(BlockGetter level, BlockPos pos, BooleanSupplier sideSurvives) {
        BlockPos upPos = pos.above();
        BlockState previous = level.getBlockState(upPos);
        boolean carpetAbove = previous.is(Blocks.PALE_MOSS_CARPET);
        if (carpetAbove && previous.getValue(BASE) || !carpetAbove && !previous.canBeReplaced()) {
            return Blocks.AIR.defaultBlockState();
        }
        BlockState topper = getUpdatedState(Blocks.PALE_MOSS_CARPET.defaultBlockState().setValue(BASE, false), level, upPos, true);
        for (Direction dir : Direction.Plane.HORIZONTAL) {
            EnumProperty<WallSide> prop = getPropertyForFace(dir);
            if (topper.getValue(prop) != WallSide.NONE && !sideSurvives.getAsBoolean()) {
                topper = topper.setValue(prop, WallSide.NONE);
            }
        }
        return hasFaces(topper) && topper != previous ? topper : Blocks.AIR.defaultBlockState();
    }
    public static final BooleanProperty BASE = net.minecraft.world.level.block.state.properties.BlockStateProperties.BOTTOM;
    public static final EnumProperty<WallSide> NORTH = net.minecraft.world.level.block.state.properties.BlockStateProperties.NORTH_WALL;
    public static final EnumProperty<WallSide> EAST = net.minecraft.world.level.block.state.properties.BlockStateProperties.EAST_WALL;
    public static final EnumProperty<WallSide> SOUTH = net.minecraft.world.level.block.state.properties.BlockStateProperties.SOUTH_WALL;
    public static final EnumProperty<WallSide> WEST = net.minecraft.world.level.block.state.properties.BlockStateProperties.WEST_WALL;
    public static final Map<Direction, EnumProperty<WallSide>> PROPERTY_BY_DIRECTION = sideMap();
    public MossyCarpetBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public Function<BlockState, VoxelShape> makeShapes() {
        Map<Direction, VoxelShape> low = Shapes.rotateHorizontal(Block.boxZ(16.0, 0.0, 10.0, 0.0, 1.0));
        Map<Direction, VoxelShape> tall = Shapes.rotateAll(Block.boxZ(16.0, 0.0, 1.0));
        return getShapeForEachState(state -> {
            VoxelShape shape = state.getValue(BASE) ? tall.get(Direction.DOWN) : Shapes.empty();
            for (Map.Entry<Direction, EnumProperty<WallSide>> entry : PROPERTY_BY_DIRECTION.entrySet()) {
                WallSide kind = state.getValue(entry.getValue());
                if (kind == WallSide.LOW) {
                    shape = Shapes.or(shape, low.get(entry.getKey()));
                } else if (kind == WallSide.TALL) {
                    shape = Shapes.or(shape, tall.get(entry.getKey()));
                }
            }
            return shape.isEmpty() ? Shapes.block() : shape;
        });
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected VoxelShape getCollisionShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getCollisionShape(state, level, pos, context);
    }
    protected boolean propagatesSkylightDown(BlockState state) {
        return super.propagatesSkylightDown(state);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public static void placeAt(LevelAccessor level, BlockPos pos, RandomSource random, int updateType) {
        BlockState layer = getUpdatedState(Blocks.PALE_MOSS_CARPET.defaultBlockState(), level, pos, true);
        level.setBlock(pos, layer, updateType);
        BlockState topper = topperWithSideChance(level, pos, random::nextBoolean);
        if (!topper.isAir()) {
            level.setBlock(pos.above(), topper, updateType);
            level.setBlock(pos, getUpdatedState(layer, level, pos, true), updateType);
        }
    }
    public void setPlacedBy(Level level, BlockPos pos, BlockState state, LivingEntity by, ItemStack itemStack) {
        super.setPlacedBy(level, pos, state, by, itemStack);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return super.rotate(state, rotation);
    }
    protected BlockState mirror(BlockState state, Mirror mirror) {
        return super.mirror(state, mirror);
    }
    public static EnumProperty<WallSide> getPropertyForFace(Direction direction) {
        return PROPERTY_BY_DIRECTION.get(direction);
    }
    public boolean isValidBonemealTarget(LevelReader level, BlockPos pos, BlockState state, BonemealSource source) {
        return state.getValue(BASE) && !topperWithSideChance(level, pos, () -> true).isAir();
    }
    public boolean isBonemealSuccess(Level level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        return true;


    }
    public void performBonemeal(ServerLevel level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        org.basaltmc.bukkit.WorldEnvNatives.boneMeal(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
    }
}
