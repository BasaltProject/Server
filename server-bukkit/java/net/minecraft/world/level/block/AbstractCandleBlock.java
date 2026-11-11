// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractCandleBlock extends Block {
    private static void setLit(LevelAccessor level, BlockState state, BlockPos pos, boolean lit) {
        level.setBlock(pos, state.setValue(LIT, lit), 11);
    }
    public static final int LIGHT_PER_CANDLE = 3;
    public static final BooleanProperty LIT = net.minecraft.world.level.block.state.properties.BlockStateProperties.LIT;
    protected AbstractCandleBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected abstract Iterable<Vec3> getParticleOffsets(BlockState state);
    public static boolean isLit(BlockState state) {
        return state.hasProperty(LIT) && (state.is(net.minecraft.tags.BlockTags.CANDLES) || state.is(net.minecraft.tags.BlockTags.CANDLE_CAKES)) && state.getValue(LIT);
    }
    protected void onProjectileHit(Level level, BlockState state, BlockHitResult blockHit, Projectile projectile) {
        super.onProjectileHit(level, state, blockHit, projectile);
    }
    protected boolean canBeLit(BlockState state) {
        return !state.getValue(LIT);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    public static void extinguish(Player player, BlockState state, LevelAccessor level, BlockPos pos) {
        setLit(level, state, pos, false);
        if (state.getBlock() instanceof AbstractCandleBlock candle) {
            candle.getParticleOffsets(state).forEach(offset -> level.addParticle(net.minecraft.core.particles.ParticleTypes.SMOKE,
                pos.getX() + offset.x(), pos.getY() + offset.y(), pos.getZ() + offset.z(), 0.0, 0.1, 0.0));
        }
        level.playSound(null, pos, net.minecraft.sounds.SoundEvents.CANDLE_EXTINGUISH, net.minecraft.sounds.SoundSource.BLOCKS, 1.0F, 1.0F);
        level.gameEvent(player, net.minecraft.world.level.gameevent.GameEvent.BLOCK_CHANGE, pos);
    }
    protected void onExplosionHit(BlockState state, ServerLevel level, BlockPos pos, Explosion explosion, BiConsumer<ItemStack, BlockPos> onHit) {
        super.onExplosionHit(state, level, pos, explosion, onHit);
    }
}
