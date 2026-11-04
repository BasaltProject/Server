// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BlockSetType;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class ButtonBlock extends FaceAttachedHorizontalDirectionalBlock {
    protected ButtonBlock(int index) {
        super(index);
    }
    public static final BooleanProperty POWERED = BlockStateProperties.POWERED;
    protected ButtonBlock(BlockSetType type, int ticksToStayPressed, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected InteractionResult useWithoutItem(BlockState state, Level level, BlockPos pos, Player player, BlockHitResult hitResult) {
        return super.useWithoutItem(state, level, pos, player, hitResult);
    }
    protected void onExplosionHit(BlockState state, ServerLevel level, BlockPos pos, Explosion explosion, BiConsumer<ItemStack, BlockPos> onHit) {
        super.onExplosionHit(state, level, pos, explosion, onHit);
    }
    public void press(BlockState state, Level level, BlockPos pos, Player player) {
        level.setBlock(pos, state.setValue(POWERED, true), 3);
        playSound(player, level, pos, true);
        if (level instanceof net.minecraft.server.level.ServerLevel server) {
            server.scheduleTick(pos, this, blockKey().getPath().contains("stone") ? 20 : 30);
        }
    }
    protected void playSound(Player player, LevelAccessor level, BlockPos pos, boolean pressed) {
        level.playSound(pressed ? player : null, pos, getSound(pressed), net.minecraft.sounds.SoundSource.BLOCKS);
    }
    protected SoundEvent getSound(boolean pressed) {
        String path = blockKey().getPath();
        if (path.contains("stone")) {
            return pressed ? net.minecraft.sounds.SoundEvents.STONE_BUTTON_CLICK_ON : net.minecraft.sounds.SoundEvents.STONE_BUTTON_CLICK_OFF;
        }
        if (path.startsWith("crimson") || path.startsWith("warped")) {
            return pressed ? net.minecraft.sounds.SoundEvents.NETHER_WOOD_BUTTON_CLICK_ON : net.minecraft.sounds.SoundEvents.NETHER_WOOD_BUTTON_CLICK_OFF;
        }
        if (path.startsWith("bamboo")) {
            return pressed ? net.minecraft.sounds.SoundEvents.BAMBOO_WOOD_BUTTON_CLICK_ON : net.minecraft.sounds.SoundEvents.BAMBOO_WOOD_BUTTON_CLICK_OFF;
        }
        if (path.startsWith("cherry")) {
            return pressed ? net.minecraft.sounds.SoundEvents.CHERRY_WOOD_BUTTON_CLICK_ON : net.minecraft.sounds.SoundEvents.CHERRY_WOOD_BUTTON_CLICK_OFF;
        }
        return pressed ? net.minecraft.sounds.SoundEvents.WOODEN_BUTTON_CLICK_ON : net.minecraft.sounds.SoundEvents.WOODEN_BUTTON_CLICK_OFF;
    }
    protected void affectNeighborsAfterRemoval(BlockState state, ServerLevel level, BlockPos pos, boolean movedByPiston) {
        super.affectNeighborsAfterRemoval(state, level, pos, movedByPiston);
    }
    protected int getDirectSignal(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return super.getDirectSignal(state, level, pos, direction);
    }
    protected boolean isSignalSource(BlockState state) {
        return super.isSignalSource(state);
    }
    protected int ownSignal(BlockState state, BlockGetter level, BlockPos pos) {
        return super.ownSignal(state, level, pos);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
        super.entityInside(state, level, pos, entity, effectApplier, isPrecise);
    }
    protected void checkPressed(BlockState state, Level level, BlockPos pos) {
        if (!blockKey().getPath().contains("stone") && !level.getEntitiesOfClass(
                net.minecraft.world.entity.projectile.arrow.AbstractArrow.class, new net.minecraft.world.phys.AABB(pos), e -> true).isEmpty()
            != state.getValue(POWERED)) {
            level.setBlock(pos, state.setValue(POWERED, !state.getValue(POWERED)), 3);
            playSound(null, level, pos, !state.getValue(POWERED));
        }
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
}
