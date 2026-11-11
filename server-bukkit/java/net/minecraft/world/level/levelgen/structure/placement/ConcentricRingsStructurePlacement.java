// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.placement;
import net.minecraft.world.level.ChunkPos;
import java.util.List;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
public class ConcentricRingsStructurePlacement extends AbstractSpreadingStructurePlacement {
    private final int distance;
    private final int spread;
    private final int count;
    private final HolderSet<Biome> preferredBiomes;
    public static final MapCodec<ConcentricRingsStructurePlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.placement.ConcentricRingsStructurePlacement.class, "CODEC");
    public ConcentricRingsStructurePlacement(Vec3i locateOffset, AbstractSpreadingStructurePlacement.FrequencyReductionMethod frequencyReductionMethod, float frequency, int salt, Optional<AbstractSpreadingStructurePlacement.ExclusionZone> exclusionZone, int distance, int spread, int count, HolderSet<Biome> preferredBiomes) {
        super(locateOffset, frequencyReductionMethod, frequency, salt, exclusionZone);
        this.distance = distance;
        this.spread = spread;
        this.count = count;
        this.preferredBiomes = preferredBiomes;
    }
    public ConcentricRingsStructurePlacement(int distance, int spread, int count, HolderSet<Biome> preferredBiomes) {
        this(Vec3i.ZERO, AbstractSpreadingStructurePlacement.FrequencyReductionMethod.DEFAULT, 1.0F, 0, Optional.empty(), distance, spread, count, preferredBiomes);
    }
    public int distance() {
        return distance;
    }
    public int spread() {
        return spread;
    }
    public int count() {
        return count;
    }
    public HolderSet<Biome> preferredBiomes() {
        return preferredBiomes;
    }
    protected boolean isPlacementChunk(ChunkGeneratorStructureState generatorState, int sourceX, int sourceZ) {
        List<ChunkPos> rings = generatorState.getRingPositionsFor(this);
        return rings != null && rings.contains(new ChunkPos(sourceX, sourceZ));
    }
    public MapCodec<ConcentricRingsStructurePlacement> codec() {
        return CODEC;
    }
}
