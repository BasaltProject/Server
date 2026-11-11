// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import net.minecraft.world.entity.player.Player;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public class SimpleExplosionDamageCalculator extends ExplosionDamageCalculator {
    private final boolean explodesBlocks;
    private final boolean damagesEntities;
    private final Optional<Float> knockbackMultiplier;
    private final Optional<HolderSet<Block>> immuneBlocks;
    public SimpleExplosionDamageCalculator(boolean explodesBlocks, boolean damagesEntities, Optional<Float> knockbackMultiplier, Optional<HolderSet<Block>> immuneBlocks) {
        this.explodesBlocks = explodesBlocks;
        this.damagesEntities = damagesEntities;
        this.knockbackMultiplier = knockbackMultiplier;
        this.immuneBlocks = immuneBlocks;
    }
    public Optional<Float> getBlockExplosionResistance(Explosion explosion, BlockGetter level, BlockPos pos, BlockState block, FluidState fluid) {
        if (immuneBlocks.isEmpty()) {
            return super.getBlockExplosionResistance(explosion, level, pos, block, fluid);
        }
        return block.is(immuneBlocks.get()) ? Optional.of(3600000.0F) : Optional.empty();
    }
    public boolean shouldBlockExplode(Explosion explosion, BlockGetter level, BlockPos pos, BlockState state, float power) {
        return explodesBlocks;
    }
    public boolean shouldDamageEntity(Explosion explosion, Entity entity) {
        return damagesEntities;
    }
    public float getKnockbackMultiplier(Entity entity) {
        if (entity instanceof Player player && player.getAbilities().flying) {
            return 0.0F;
        }
        return knockbackMultiplier.orElseGet(() -> super.getKnockbackMultiplier(entity));
    }
}
