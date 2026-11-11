// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.InclusiveRange;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public class DualNoiseProvider extends NoiseProvider {
    private final net.minecraft.util.InclusiveRange<Integer> variety;
    private final NormalNoise slowNoiseParameters;
    private final float slowScale;
    private final Noise slowNoise;
    public static final MapCodec<DualNoiseProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.DualNoiseProvider.class, "CODEC");
    public DualNoiseProvider(InclusiveRange<Integer> variety, NormalNoise slowNoiseParameters, float slowScale, long seed, NormalNoise parameters, float scale, List<BlockState> states) {
        super(seed, parameters, scale, states);
        this.variety = variety;
        this.slowNoiseParameters = slowNoiseParameters;
        this.slowScale = slowScale;
        this.slowNoise = slowNoiseParameters.create(new net.minecraft.world.level.levelgen.WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(seed)));
    }
    public MapCodec<DualNoiseProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        int count = (int) Mth.clampedMap(getSlowNoiseValue(pos), -1.0, 1.0, variety.minInclusive(), variety.maxInclusive() + 1);
        List<BlockState> picked = new java.util.ArrayList<>(count);
        for (int at = 0; at < count; at++) {
            picked.add(getRandomState(states, getSlowNoiseValue(pos.offset(at * 54545, 0, at * 34234))));
        }
        return getRandomState(picked, pos, scale);
    }
    protected float getSlowNoiseValue(BlockPos pos) {
        return (float) slowNoise.get(pos.getX() * slowScale, pos.getY() * slowScale, pos.getZ() * slowScale);
    }
}
