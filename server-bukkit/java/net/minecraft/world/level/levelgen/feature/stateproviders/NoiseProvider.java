// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import net.minecraft.util.Mth;
import com.mojang.datafixers.Products;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public class NoiseProvider extends NoiseBasedStateProvider {
    public static final MapCodec<NoiseProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.NoiseProvider.class, "CODEC");
    protected final List<BlockState> states;
    protected static <P extends NoiseProvider> Products.P4<RecordCodecBuilder.Mu<P>, Long, NormalNoise, Float, List<BlockState>> noiseProviderCodec(RecordCodecBuilder.Instance<P> instance) {
        return noiseCodec(instance).and(net.minecraft.util.ExtraCodecs.nonEmptyList(BlockState.CODEC.listOf()).fieldOf("states").forGetter(p -> p.states));
    }
    public NoiseProvider(long seed, NormalNoise parameters, float scale, List<BlockState> states) {
        super(seed, parameters, scale);
        this.states = states;
    }
    public MapCodec<? extends NoiseProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        return getRandomState(states, pos, scale);
    }
    protected BlockState getRandomState(List<BlockState> states, BlockPos pos, double scale) {
        return getRandomState(states, getNoiseValue(pos, scale));
    }
    protected BlockState getRandomState(List<BlockState> states, float noiseValue) {
        float placement = Mth.clamp((1.0F + noiseValue) / 2.0F, 0.0F, 0.9999F);
        return states.get((int) (placement * (float) states.size()));
    }
}
