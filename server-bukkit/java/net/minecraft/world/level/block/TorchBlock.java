// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.particles.SimpleParticleType;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class TorchBlock extends BaseTorchBlock {
    protected TorchBlock(int index) {
        super(index);
    }
    protected static final MapCodec<SimpleParticleType> PARTICLE_OPTIONS_FIELD = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.block.TorchBlock.class, "PARTICLE_OPTIONS_FIELD");
    protected final SimpleParticleType flameParticle = null;
    protected TorchBlock(SimpleParticleType flameParticle, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
}
