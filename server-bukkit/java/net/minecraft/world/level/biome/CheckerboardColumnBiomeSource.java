// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import com.mojang.serialization.MapCodec;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
public class CheckerboardColumnBiomeSource extends BiomeSource implements BiomeResolver {
    public static final MapCodec<CheckerboardColumnBiomeSource> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        Biome.LIST_CODEC.fieldOf("biomes").forGetter(source -> source.allowedBiomes),
        Codec.intRange(0, 62).optionalFieldOf("scale", 2).forGetter(source -> source.size)).apply(i, CheckerboardColumnBiomeSource::new));
    private final HolderSet<Biome> allowedBiomes;
    private final int bitShift;
    private final int size;
    public CheckerboardColumnBiomeSource(HolderSet<Biome> allowedBiomes, int size) {
        this.allowedBiomes = allowedBiomes;
        this.bitShift = size + 2;
        this.size = size;
    }
    @Override
    protected Stream<Holder<Biome>> collectPossibleBiomes() {
        return allowedBiomes.stream();
    }
    @Override
    public BiomeResolver createResolver(Climate.Sampler sampler) {
        return this;
    }
    @Override
    protected MapCodec<CheckerboardColumnBiomeSource> codec() {
        return CODEC;
    }
    @Override
    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return allowedBiomes.get(Math.floorMod((quartX >> bitShift) + (quartZ >> bitShift), allowedBiomes.size()));
    }
}
