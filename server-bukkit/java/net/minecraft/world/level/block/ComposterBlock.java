// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.component.Compostable;
import net.minecraft.world.level.gameevent.GameEvent;
import java.util.Optional;
import net.minecraft.world.phys.Vec3;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.WorldlyContainer;
import net.minecraft.world.WorldlyContainerHolder;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class ComposterBlock extends Block implements WorldlyContainerHolder {
    private static BlockState empty(Entity sourceEntity, BlockState state, LevelAccessor level, BlockPos pos) {
        BlockState newState = state.setValue(LEVEL, 0);
        level.setBlock(pos, newState, 3);
        announce(level, pos, sourceEntity, newState);
        return newState;
    }
    private static void announce(LevelAccessor level, BlockPos pos, Entity source, BlockState newState) {
        level.gameEvent(GameEvent.BLOCK_CHANGE, pos, GameEvent.Context.of(source, newState));
    }
    private static BlockState addLayer(Entity sourceEntity, BlockState state, ServerLevel level, BlockPos pos, Compostable compostable) {
        int fillLevel = state.getValue(LEVEL);
        net.minecraft.world.level.storage.loot.LootParams params = new net.minecraft.world.level.storage.loot.LootParams.Builder(level)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.BLOCK_STATE, state)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN, Vec3.atCenterOf(pos))
            .withOptionalParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.INTERACTING_ENTITY, sourceEntity)
            .create(net.minecraft.world.level.storage.loot.parameters.LootContextParamSets.BLOCK_INTERACT);
        net.minecraft.world.level.storage.loot.LootContext context = new net.minecraft.world.level.storage.loot.LootContext.Builder(params).create(Optional.empty());
        int layers = compostable.layers().get(context, 0);
        if (layers <= 0) {
            return state;
        }
        int newLevel = net.minecraft.util.Mth.clamp(fillLevel + layers, 0, 7);
        BlockState newState = state.setValue(LEVEL, newLevel);
        level.setBlock(pos, newState, 3);
        announce(level, pos, sourceEntity, newState);
        if (newLevel == 7) {
            level.scheduleTick(pos, state.getBlock(), 20);
        }
        return newState;
    }
    public static final int READY = 8;
    public static final int MIN_LEVEL = 0;
    public static final int MAX_LEVEL = 7;
    public static final IntegerProperty LEVEL = net.minecraft.world.level.block.state.properties.BlockStateProperties.LEVEL_COMPOSTER;
    public ComposterBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public static void handleFill(Level level, BlockPos pos, boolean success) {
        BlockState atPos = level.getBlockState(pos);
        level.playLocalSound(pos, success ? SoundEvents.COMPOSTER_FILL_SUCCESS : SoundEvents.COMPOSTER_FILL, SoundSource.BLOCKS, 1.0F, 1.0F, false);
        double centerHeight = atPos.getShape(level, pos).max(Direction.Axis.Y, 0.5, 0.5) + 0.03125;
        RandomSource dice = level.getRandom();
        for (int i = 0; i < 10; i++) {
            double x = pos.getX() + 0.1875 + 0.625 * dice.nextFloat();
            double y = pos.getY() + centerHeight + dice.nextFloat() * (1.0 - centerHeight);
            double z = pos.getZ() + 0.1875 + 0.625 * dice.nextFloat();
            level.addParticle(ParticleTypes.COMPOSTER, x, y, z, dice.nextGaussian() * 0.02, dice.nextGaussian() * 0.02, dice.nextGaussian() * 0.02);
        }
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected VoxelShape getInteractionShape(BlockState state, BlockGetter level, BlockPos pos) {
        return super.getInteractionShape(state, level, pos);
    }
    protected VoxelShape getCollisionShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getCollisionShape(state, level, pos, context);
    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    protected InteractionResult useItemOn(ItemStack itemStack, BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, BlockHitResult hitResult) {
        return super.useItemOn(itemStack, state, level, pos, player, hand, hitResult);
    }
    protected InteractionResult useWithoutItem(BlockState state, Level level, BlockPos pos, Player player, BlockHitResult hitResult) {
        return super.useWithoutItem(state, level, pos, player, hitResult);
    }
    public static BlockState insertItem(Entity sourceEntity, BlockState state, ServerLevel level, ItemStack itemStack, BlockPos pos) {
        Compostable compostable = itemStack.get(DataComponents.COMPOSTABLE);
        if (state.getValue(LEVEL) < 7 && compostable != null) {
            BlockState newState = addLayer(sourceEntity, state, level, pos, compostable);
            itemStack.shrink(1);
            return newState;
        }
        return state;
    }
    public static BlockState extractProduce(Entity sourceEntity, BlockState state, Level level, BlockPos pos) {
        if (!level.isClientSide()) {
            Vec3 itemPos = Vec3.atLowerCornerWithOffset(pos, 0.5, 1.01, 0.5).offsetRandomXZ(level.getRandom(), 0.7F);
            net.minecraft.world.entity.item.ItemEntity entity = new net.minecraft.world.entity.item.ItemEntity(level, itemPos.x(), itemPos.y(), itemPos.z(), new ItemStack(Items.BONE_MEAL));
            entity.setDefaultPickUpDelay();
            level.addFreshEntity(entity);
        }
        BlockState emptied = empty(sourceEntity, state, level, pos);
        level.playSound(null, pos, SoundEvents.COMPOSTER_EMPTY, SoundSource.BLOCKS, 1.0F, 1.0F);
        return emptied;
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected boolean hasAnalogOutputSignal(BlockState state) {
        return super.hasAnalogOutputSignal(state);
    }
    protected int getAnalogOutputSignal(BlockState state, Level level, BlockPos pos, Direction direction) {
        return super.getAnalogOutputSignal(state, level, pos, direction);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return super.isPathfindable(state, type);
    }
    public WorldlyContainer getContainer(BlockState state, LevelAccessor level, BlockPos pos) {
        int content = state.getValue(LEVEL);
        if (content == 8) {
            return new OutputContainer(state, level, pos, new ItemStack(Items.BONE_MEAL));
        }
        return content < 7 ? new InputContainer(state, level, pos) : new EmptyContainer();
    }
    static class EmptyContainer extends SimpleContainer implements WorldlyContainer {
        public EmptyContainer() {
            super((int) 0);
        }
        public int[] getSlotsForFace(Direction direction) {
            return new int[0];
        }
        public boolean canPlaceItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
            return false;
        }
        public boolean canTakeItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
            return false;
        }
    }
static class OutputContainer extends SimpleContainer implements WorldlyContainer {
        private BlockState state;
        private LevelAccessor level;
        private BlockPos pos;
        private boolean changed;
        public OutputContainer(BlockState state, LevelAccessor level, BlockPos pos, ItemStack contents) {
            super(1);
            setItem(0, contents);
            this.state = state;
            this.level = level;
            this.pos = pos;
        }
        public int getMaxStackSize() {
            return 1;
        }
        public int[] getSlotsForFace(Direction direction) {
            return direction == Direction.DOWN ? new int[] {0} : new int[0];
        }
        public boolean canPlaceItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
            return false;
        }
        public boolean canTakeItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
            return !changed && direction == Direction.DOWN && itemStack.is(Items.BONE_MEAL);
        }
        public void setChanged() {
            ComposterBlock.empty(null, state, level, pos);
            changed = true;
        }
    }
static class InputContainer extends SimpleContainer implements WorldlyContainer {
        private BlockState state;
        private LevelAccessor level;
        private BlockPos pos;
        private boolean changed;
        public InputContainer(BlockState state, LevelAccessor level, BlockPos pos) {
            super(1);
            this.state = state;
            this.level = level;
            this.pos = pos;
        }
        public int getMaxStackSize() {
            return 1;
        }
        public int[] getSlotsForFace(Direction direction) {
            return direction == Direction.UP ? new int[] {0} : new int[0];
        }
        public boolean canPlaceItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
            return !changed && direction == Direction.UP && itemStack.has(DataComponents.COMPOSTABLE);
        }
        public boolean canTakeItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
            return false;
        }
        public void setChanged() {
            ItemStack contents = getItem(0);
            Compostable compostable = contents.get(DataComponents.COMPOSTABLE);
            if (!contents.isEmpty() && compostable != null && level instanceof ServerLevel serverLevel) {
                changed = true;
                BlockState newState = ComposterBlock.addLayer(null, state, serverLevel, pos, compostable);
                level.levelEvent(1500, pos, newState != state ? 1 : 0);
                removeItemNoUpdate(0);
            }
        }
    }
}
