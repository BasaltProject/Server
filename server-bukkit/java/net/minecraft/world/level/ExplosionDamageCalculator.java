// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public class ExplosionDamageCalculator {
    public Optional<Float> getBlockExplosionResistance(Explosion explosion, BlockGetter level, BlockPos pos, BlockState block, FluidState fluid) {
        if (block.isAir() && fluid.isEmpty()) {
            return Optional.empty();
        }
        return Optional.of(Math.max(block.getBlock().getExplosionResistance(), fluid.getExplosionResistance()));
    }
    public boolean shouldBlockExplode(Explosion explosion, BlockGetter level, BlockPos pos, BlockState state, float power) {
        return true;
    }
    public boolean shouldDamageEntity(Explosion explosion, Entity entity) {
        return true;
    }
    public float getKnockbackMultiplier(Entity entity) {
        return 1.0F;
    }
    public float getEntityDamageAmount(Explosion explosion, Entity entity, float exposure) {
        float diameter = explosion.radius() * 2.0F;
        double falloff = Math.sqrt(entity.distanceToSqr(explosion.center())) / diameter;
        double impact = (1.0 - falloff) * exposure;
        return (float) ((impact * impact + impact) / 2.0 * 7.0 * diameter + 1.0);
    }
}
