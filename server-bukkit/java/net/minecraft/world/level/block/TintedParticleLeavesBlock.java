// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
public class TintedParticleLeavesBlock extends FallingParticlesLeavesBlock {
    public TintedParticleLeavesBlock(float leafParticleChance, BlockBehaviour.Properties properties) {
        super((float) 0, (net.minecraft.world.level.block.sounds.AmbientLeavesBlockSoundPlayer) null, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected void spawnFallingLeavesParticle(Level level, BlockPos pos, RandomSource random) {
        return;
    }
}
