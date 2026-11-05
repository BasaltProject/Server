// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class CropBlock extends VegetationBlock implements BonemealableBlock {
    protected CropBlock(int index) {
        super(index);
    }
    public static final int MAX_AGE = 7;
    public static final IntegerProperty AGE = BlockStateProperties.AGE_7;
    protected CropBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean mayPlaceOn(BlockState state, BlockGetter level, BlockPos pos) {
        return super.mayPlaceOn(state, level, pos);
    }
    protected IntegerProperty getAgeProperty() {
        for (net.minecraft.world.level.block.state.properties.Property<?> property : defaultBlockState().getProperties()) {
            if (property instanceof IntegerProperty integer && property.getName().equals("age")) {
                return integer;
            }
        }
        return BlockStateProperties.AGE_7;
    }
    public int getMaxAge() {
        return java.util.Collections.max(getAgeProperty().getPossibleValues());
    }
    public int getAge(BlockState state) {
        return state.getValue(getAgeProperty());
    }
    public BlockState getStateForAge(int age) {
        return defaultBlockState().setValue(getAgeProperty(), age);
    }
    public final boolean isMaxAge(BlockState state) {
        return getAge(state) >= getMaxAge();
    }
    protected boolean isRandomlyTicking(BlockState state) {
        return super.isRandomlyTicking(state);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    public void growCrops(Level level, BlockPos pos, BlockState state) {
        level.setBlock(pos, getStateForAge(Math.min(getAge(state) + getBonemealAgeIncrease(level), getMaxAge())), 2);
    }
    protected int getBonemealAgeIncrease(Level level) {
        return 2 + java.util.concurrent.ThreadLocalRandom.current().nextInt(4);
    }
    protected static float getGrowthSpeed(Block type, BlockGetter level, BlockPos pos) {
        float speed = 1.0F;
        BlockPos below = pos.below();
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                BlockState ground = level.getBlockState(below.offset(dx, 0, dz));
                float share = 0.0F;
                if (ground.is(net.minecraft.tags.BlockTags.GROWS_CROPS)) {
                    share = ground.getValueOrElse(FarmlandBlock.MOISTURE, 0) > 0 ? 3.0F : 1.0F;
                }
                speed += dx == 0 && dz == 0 ? share : share / 4.0F;
            }
        }
        BlockPos west = pos.west();
        BlockPos east = pos.east();
        BlockPos north = pos.north();
        BlockPos south = pos.south();
        boolean alongX = level.getBlockState(west).is(type) || level.getBlockState(east).is(type);
        boolean alongZ = level.getBlockState(north).is(type) || level.getBlockState(south).is(type);
        boolean diagonal = level.getBlockState(west.north()).is(type) || level.getBlockState(east.north()).is(type)
            || level.getBlockState(east.south()).is(type) || level.getBlockState(west.south()).is(type);
        return (alongX && alongZ) || diagonal ? speed / 2.0F : speed;
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected static boolean hasSufficientLight(LevelReader level, BlockPos pos) {
        return level.getRawBrightness(pos, 0) >= 8;
    }
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
        super.entityInside(state, level, pos, entity, effectApplier, isPrecise);
    }
    protected ItemLike getBaseSeedId() {
        String name = net.minecraft.core.registries.BuiltInRegistries.BLOCK.getKey(this).getPath();
        return switch (name) {
            case "carrots" -> net.minecraft.world.item.Items.CARROT;
            case "potatoes" -> net.minecraft.world.item.Items.POTATO;
            case "beetroots" -> net.minecraft.world.item.Items.BEETROOT_SEEDS;
            default -> net.minecraft.world.item.Items.WHEAT_SEEDS;
        };
    }
    protected ItemStack getCloneItemStack(LevelReader level, BlockPos pos, BlockState state, boolean includeData) {
        return super.getCloneItemStack(level, pos, state, includeData);
    }
    public boolean isValidBonemealTarget(LevelReader level, BlockPos pos, BlockState state, BonemealSource source) {
        return !isMaxAge(state);
    }
    public boolean isBonemealSuccess(Level level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        return true;
    }
    public void performBonemeal(ServerLevel level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        growCrops(level, pos, state);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
}
