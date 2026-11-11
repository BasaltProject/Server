// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.serialization.Codec;
import com.mojang.datafixers.Products;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public abstract class NoiseBasedStateProvider implements BlockStateProvider {
    protected final long seed;
    protected final NormalNoise parameters;
    protected final float scale;
    protected final Noise noise;
    protected static <P extends NoiseBasedStateProvider> Products.P3<RecordCodecBuilder.Mu<P>, Long, NormalNoise, Float> noiseCodec(RecordCodecBuilder.Instance<P> instance) {
        return instance.group(Codec.LONG.fieldOf("seed").forGetter(p -> p.seed), NormalNoise.DIRECT_CODEC.fieldOf("noise").forGetter(p -> p.parameters),
            net.minecraft.util.ExtraCodecs.POSITIVE_FLOAT.fieldOf("scale").forGetter(p -> p.scale));
    }
    public abstract MapCodec<? extends NoiseBasedStateProvider> codec();
    protected NoiseBasedStateProvider(long seed, NormalNoise parameters, float scale) {
        this.seed = seed;
        this.parameters = parameters;
        this.scale = scale;
        this.noise = parameters.create(new net.minecraft.world.level.levelgen.WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(seed)));
    }
    protected float getNoiseValue(BlockPos pos, double scale) {
        return (float) noise.get(pos.getX() * scale, pos.getY() * scale, pos.getZ() * scale);
    }
}
