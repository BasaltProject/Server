// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import com.mojang.serialization.MapCodec;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.QuartPos;
import net.minecraft.core.SectionPos;
public class TheEndBiomeSource extends BiomeSource {
    public static final MapCodec<TheEndBiomeSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.biome.TheEndBiomeSource.class, "CODEC");
    private final Holder<Biome> end;
    private final Holder<Biome> highlands;
    private final Holder<Biome> midlands;
    private final Holder<Biome> islands;
    private final Holder<Biome> barrens;
    public static TheEndBiomeSource create(HolderGetter<Biome> biomes) {
        return new TheEndBiomeSource(biomes.getOrThrow(Biomes.THE_END), biomes.getOrThrow(Biomes.END_HIGHLANDS), biomes.getOrThrow(Biomes.END_MIDLANDS),
            biomes.getOrThrow(Biomes.SMALL_END_ISLANDS), biomes.getOrThrow(Biomes.END_BARRENS));
    }
    private TheEndBiomeSource(Holder<Biome> end, Holder<Biome> highlands, Holder<Biome> midlands, Holder<Biome> islands, Holder<Biome> barrens) {
        this.end = end;
        this.highlands = highlands;
        this.midlands = midlands;
        this.islands = islands;
        this.barrens = barrens;
    }
    @Override
    protected Stream<Holder<Biome>> collectPossibleBiomes() {
        return Stream.of(end, highlands, midlands, islands, barrens);
    }
    @Override
    protected MapCodec<TheEndBiomeSource> codec() {
        return CODEC;
    }
    @Override
    public BiomeResolver createResolver(Climate.Sampler sampler) {
        return (quartX, quartY, quartZ) -> pick(quartX, quartY, quartZ, sampler);
    }
    private Holder<Biome> pick(int quartX, int quartY, int quartZ, Climate.Sampler sampler) {
        int blockX = QuartPos.toBlock(quartX);
        int blockY = QuartPos.toBlock(quartY);
        int blockZ = QuartPos.toBlock(quartZ);
        long chunkX = SectionPos.blockToSectionCoord(blockX);
        long chunkZ = SectionPos.blockToSectionCoord(blockZ);
        if (chunkX * chunkX + chunkZ * chunkZ <= 4096L) {
            return end;
        }
        int islandX = (SectionPos.blockToSectionCoord(blockX) * 2 + 1) * 8;
        int islandZ = (SectionPos.blockToSectionCoord(blockZ) * 2 + 1) * 8;
        double height = Climate.unquantizeCoord(sampler.sample(QuartPos.fromBlock(islandX), QuartPos.fromBlock(blockY), QuartPos.fromBlock(islandZ)).erosion());
        if (height > 0.25) {
            return highlands;
        }
        if (height >= -0.0625) {
            return midlands;
        }
        return height < -0.21875 ? islands : barrens;
    }
}
