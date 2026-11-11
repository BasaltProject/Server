// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.sounds.AmbientLeavesBlockSoundPlayer;
import net.minecraft.world.level.block.state.BlockBehaviour;
public class UntintedParticleLeavesBlock extends FallingParticlesLeavesBlock {
    protected final ParticleOptions leafParticle = null;
    public UntintedParticleLeavesBlock(float leafParticleChance, ParticleOptions leafParticle, AmbientLeavesBlockSoundPlayer ambientLeavesBlockSoundPlayer, BlockBehaviour.Properties properties) {
        super((float) 0, (net.minecraft.world.level.block.sounds.AmbientLeavesBlockSoundPlayer) null, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected void spawnFallingLeavesParticle(Level level, BlockPos pos, RandomSource random) {
        return;
    }
}
