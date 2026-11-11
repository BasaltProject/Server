// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import com.mojang.serialization.Lifecycle;
import com.mojang.serialization.Codec;
import net.minecraft.world.level.levelgen.NoiseRouterData;
import com.mojang.datafixers.util.Pair;
import net.minecraft.core.QuartPos;
public class MultiNoiseBiomeSource extends BiomeSource {
    private static final MapCodec<Holder<Biome>> kEntryCodec = Biome.CODEC.fieldOf("biome");
    public static final MapCodec<Climate.ParameterList<Holder<Biome>>> DIRECT_CODEC = Climate.ParameterList.codec(kEntryCodec).fieldOf("biomes");
    private static final MapCodec<Holder<MultiNoiseBiomeSourceParameterList>> kPresetCodec =
        MultiNoiseBiomeSourceParameterList.CODEC.fieldOf("preset").withLifecycle(Lifecycle.stable());
    public static final MapCodec<MultiNoiseBiomeSource> CODEC = Codec.mapEither(DIRECT_CODEC, kPresetCodec).xmap(MultiNoiseBiomeSource::new, source -> source.parameters);
    private final Either<Climate.ParameterList<Holder<Biome>>, Holder<MultiNoiseBiomeSourceParameterList>> parameters;
    private MultiNoiseBiomeSource(Either<Climate.ParameterList<Holder<Biome>>, Holder<MultiNoiseBiomeSourceParameterList>> parameters) {
        this.parameters = parameters;
    }
    public static MultiNoiseBiomeSource createFromList(Climate.ParameterList<Holder<Biome>> parameters) {
        return new MultiNoiseBiomeSource(Either.left(parameters));
    }
    public static MultiNoiseBiomeSource createFromPreset(Holder<MultiNoiseBiomeSourceParameterList> preset) {
        return new MultiNoiseBiomeSource(Either.right(preset));
    }
    private Climate.ParameterList<Holder<Biome>> list() {
        return parameters.map(direct -> direct, preset -> preset.value().parameters());
    }
    @Override
    protected Stream<Holder<Biome>> collectPossibleBiomes() {
        return list().values().stream().map(Pair::getSecond);
    }
    @Override
    protected MapCodec<MultiNoiseBiomeSource> codec() {
        return CODEC;
    }
    public boolean stable(ResourceKey<MultiNoiseBiomeSourceParameterList> expected) {
        return parameters.right().map(preset -> preset.is(expected)).orElse(false);
    }
    @Override
    public BiomeResolver createResolver(Climate.Sampler sampler) {
        return (quartX, quartY, quartZ) -> getNoiseBiome(sampler.sample(quartX, quartY, quartZ));
    }
    public BiomeResolver createResolverForChunk(Climate.Sampler sampler, int minQuartX, int minQuartY, int minQuartZ, int quartSizeX, int quartSizeY, int quartSizeZ) {
        return createResolver(sampler);
    }
    public Holder<Biome> getNoiseBiome(Climate.TargetPoint target) {
        return list().findValue(target);
    }
    public void addDebugInfo(List<String> result, BlockPos feetPos, Climate.Sampler sampler) {
        Climate.TargetPoint at = sampler.sample(QuartPos.fromBlock(feetPos.getX()), QuartPos.fromBlock(feetPos.getY()), QuartPos.fromBlock(feetPos.getZ()));
        float continentalness = Climate.unquantizeCoord(at.continentalness());
        float erosion = Climate.unquantizeCoord(at.erosion());
        float temperature = Climate.unquantizeCoord(at.temperature());
        float humidity = Climate.unquantizeCoord(at.humidity());
        float weirdness = Climate.unquantizeCoord(at.weirdness());
        OverworldBiomeBuilder builder = new OverworldBiomeBuilder();
        result.add("Biome builder PV: " + OverworldBiomeBuilder.getDebugStringForPeaksAndValleys(NoiseRouterData.peaksAndValleys(weirdness))
            + " C: " + builder.getDebugStringForContinentalness(continentalness) + " E: " + builder.getDebugStringForErosion(erosion)
            + " T: " + builder.getDebugStringForTemperature(temperature) + " H: " + builder.getDebugStringForHumidity(humidity));
    }
}
