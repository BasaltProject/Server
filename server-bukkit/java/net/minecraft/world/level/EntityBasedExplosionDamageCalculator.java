// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public class EntityBasedExplosionDamageCalculator extends ExplosionDamageCalculator {
    private final Entity source;
    public EntityBasedExplosionDamageCalculator(Entity source) {
        this.source = source;
    }
    public Optional<Float> getBlockExplosionResistance(Explosion explosion, BlockGetter level, BlockPos pos, BlockState block, FluidState fluid) {
        return super.getBlockExplosionResistance(explosion, level, pos, block, fluid)
            .map(resistance -> source.getBlockExplosionResistance(explosion, level, pos, block, fluid, resistance));
    }
    public boolean shouldBlockExplode(Explosion explosion, BlockGetter level, BlockPos pos, BlockState state, float power) {
        return source.shouldBlockExplode(explosion, level, pos, state, power);
    }
}
