// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.placement;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
public class RandomSpreadStructurePlacement extends AbstractSpreadingStructurePlacement {
    private final int spacing;
    private final int separation;
    private final RandomSpreadType spreadType;
    public static final MapCodec<RandomSpreadStructurePlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.placement.RandomSpreadStructurePlacement.class, "CODEC");
    public RandomSpreadStructurePlacement(Vec3i locateOffset, AbstractSpreadingStructurePlacement.FrequencyReductionMethod frequencyReductionMethod, float frequency, int salt, Optional<AbstractSpreadingStructurePlacement.ExclusionZone> exclusionZone, int spacing, int separation, RandomSpreadType spreadType) {
        super(locateOffset, frequencyReductionMethod, frequency, salt, exclusionZone);
        this.spacing = spacing;
        this.separation = separation;
        this.spreadType = spreadType;
    }
    public RandomSpreadStructurePlacement(int spacing, int separation, RandomSpreadType spreadType, int salt) {
        this(Vec3i.ZERO, AbstractSpreadingStructurePlacement.FrequencyReductionMethod.DEFAULT, 1.0F, salt, Optional.empty(), spacing, separation, spreadType);
    }
    public int spacing() {
        return spacing;
    }
    public int separation() {
        return separation;
    }
    public RandomSpreadType spreadType() {
        return spreadType;
    }
    public ChunkPos getPotentialStructureChunk(long seed, int sourceX, int sourceZ) {
        int gridX = Math.floorDiv(sourceX, spacing);
        int gridZ = Math.floorDiv(sourceZ, spacing);
        net.minecraft.world.level.levelgen.WorldgenRandom random = new net.minecraft.world.level.levelgen.WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(0L));
        random.setLargeFeatureWithSalt(seed, gridX, gridZ, salt());
        int limit = spacing - separation;
        int offsetX = spreadType.evaluate(random, limit);
        int offsetZ = spreadType.evaluate(random, limit);
        return new ChunkPos(gridX * spacing + offsetX, gridZ * spacing + offsetZ);
    }
    protected boolean isPlacementChunk(ChunkGeneratorStructureState state, int sourceX, int sourceZ) {
        ChunkPos chunk = getPotentialStructureChunk(state.getLevelSeed(), sourceX, sourceZ);
        return chunk.x() == sourceX && chunk.z() == sourceZ;
    }
    public MapCodec<RandomSpreadStructurePlacement> codec() {
        return CODEC;
    }
}
