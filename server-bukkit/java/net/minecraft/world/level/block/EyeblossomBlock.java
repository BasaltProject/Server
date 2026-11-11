// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.world.phys.Vec3;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class EyeblossomBlock extends FlowerBlock {
    public EyeblossomBlock(Type type, BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public EyeblossomBlock(boolean open, BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
        super.entityInside(state, level, pos, entity, effectApplier, isPrecise);
    }
    public MobEffectInstance getBeeInteractionEffect() {
        return new MobEffectInstance(MobEffects.POISON, 25);
    }
    public static enum Type {
OPEN(true, 16545810), CLOSED(false, 6250335);
        private final boolean open;
        private final int particleColor;
        Type(boolean open, int particleColor) {
            this.open = open;
            this.particleColor = particleColor;
        }
        public Block block() {
            return open ? Blocks.OPEN_EYEBLOSSOM : Blocks.CLOSED_EYEBLOSSOM;
        }
        public BlockState state() {
            return block().defaultBlockState();
        }
        public Type transform() {
            return fromBoolean(!open);
        }
        public boolean emitSounds() {
            return open;
        }
        public static Type fromBoolean(boolean open) {
            return open ? OPEN : CLOSED;
        }
        public void spawnTransformParticle(ServerLevel level, BlockPos pos, RandomSource random) {
            Vec3 start = Vec3.atCenterOf(pos);
            double lifetime = 0.5 + random.nextDouble();
            Vec3 velocity = new Vec3(random.nextDouble() - 0.5, random.nextDouble() + 1.0, random.nextDouble() - 0.5);
            Vec3 target = start.add(velocity.scale(lifetime));
            level.sendParticles(new net.minecraft.core.particles.TrailParticleOption(target, particleColor, (int) (20.0 * lifetime)),
                start.x, start.y, start.z, 1, 0.0, 0.0, 0.0, 0.0);
        }
        public SoundEvent longSwitchSound() {
            return open ? net.minecraft.sounds.SoundEvents.EYEBLOSSOM_OPEN_LONG : net.minecraft.sounds.SoundEvents.EYEBLOSSOM_CLOSE_LONG;
        }
    }
}
