// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class BaseFireBlock extends Block {
    protected BaseFireBlock(int index) {
        super(index);
    }
    protected static final VoxelShape SHAPE = null;
    public BaseFireBlock(BlockBehaviour.Properties properties, float fireDamage) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public static BlockState getState(BlockGetter level, BlockPos pos) {
        BlockState below = level.getBlockState(pos.below());
        return below.is(net.minecraft.tags.BlockTags.SOUL_FIRE_BASE_BLOCKS) ? Blocks.SOUL_FIRE.defaultBlockState() : Blocks.FIRE.defaultBlockState();
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    protected abstract boolean canBurn(BlockState state);
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
        super.entityInside(state, level, pos, entity, effectApplier, isPrecise);
    }
    public static void fireIgnite(Entity entity) {
        if (entity.fireImmune()) {
            return;
        }
        int ticks = entity.getRemainingFireTicks();
        if (ticks < 0) {
            entity.setRemainingFireTicks(ticks + 1);
        } else if (entity instanceof net.minecraft.server.level.ServerPlayer) {
            entity.setRemainingFireTicks(ticks + entity.level().getRandom().nextInt(1, 3));
        }
        if (entity.getRemainingFireTicks() >= 0) {
            entity.igniteForSeconds(8.0F);
        }
    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    public void spawnDestroyByEntityParticles(Level level, Entity entity, BlockPos pos, BlockState state) {
        super.spawnDestroyByEntityParticles(level, entity, pos, state);
    }
    public BlockState playerWillDestroy(Level level, BlockPos pos, BlockState state, Player player) {
        return super.playerWillDestroy(level, pos, state, player);
    }
    public static boolean canBePlacedAt(Level level, BlockPos pos, Direction forwardDirection) {
        BlockState here = level.getBlockState(pos);
        return here.isAir() && getState(level, pos).canSurvive(level, pos);
    }
}
