// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public class NoiseThresholdProvider extends NoiseBasedStateProvider {
    private final float threshold;
    private final float highChance;
    private final BlockState defaultState;
    private final List<BlockState> lowStates;
    private final List<BlockState> highStates;
    public static final MapCodec<NoiseThresholdProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.NoiseThresholdProvider.class, "CODEC");
    public NoiseThresholdProvider(long seed, NormalNoise parameters, float scale, float threshold, float highChance, BlockState defaultState, List<BlockState> lowStates, List<BlockState> highStates) {
        super(seed, parameters, scale);
        this.threshold = threshold;
        this.highChance = highChance;
        this.defaultState = defaultState;
        this.lowStates = lowStates;
        this.highStates = highStates;
    }
    public MapCodec<NoiseThresholdProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        double value = getNoiseValue(pos, scale);
        if (value < threshold) {
            return lowStates.get(random.nextInt(lowStates.size()));
        }
        return random.nextFloat() < highChance ? highStates.get(random.nextInt(highStates.size())) : defaultState;
    }
}
