// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.placement;
import com.mojang.datafixers.Products;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
import net.minecraft.world.level.levelgen.structure.StructureSet;
public abstract class AbstractSpreadingStructurePlacement implements StructurePlacement {
    private final Vec3i locateOffset;
    private final FrequencyReductionMethod frequencyReductionMethod;
    private final float frequency;
    private final int salt;
    private final Optional<ExclusionZone> exclusionZone;
    private static boolean defaultReducer(long seed, int salt, int sourceX, int sourceZ, float probability) {
        net.minecraft.world.level.levelgen.WorldgenRandom random = new net.minecraft.world.level.levelgen.WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(0L));
        random.setLargeFeatureWithSalt(seed, salt, sourceX, sourceZ);
        return random.nextFloat() < probability;
    }
    private static boolean doubleReducer(long seed, int salt, int sourceX, int sourceZ, float probability) {
        net.minecraft.world.level.levelgen.WorldgenRandom random = new net.minecraft.world.level.levelgen.WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(0L));
        random.setLargeFeatureSeed(seed, sourceX, sourceZ);
        return random.nextDouble() < (double) probability;
    }
    private static boolean fixedSaltReducer(long seed, int salt, int sourceX, int sourceZ, float probability) {
        net.minecraft.world.level.levelgen.WorldgenRandom random = new net.minecraft.world.level.levelgen.WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(0L));
        random.setLargeFeatureWithSalt(seed, sourceX, sourceZ, 10387320);
        return random.nextFloat() < probability;
    }
    private static boolean outpostReducer(long seed, int salt, int sourceX, int sourceZ, float probability) {
        net.minecraft.world.level.levelgen.WorldgenRandom random = new net.minecraft.world.level.levelgen.WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(0L));
        random.setSeed((long) ((sourceX >> 4) ^ ((sourceZ >> 4) << 4)) ^ seed);
        random.nextInt();
        return random.nextInt((int) (1.0F / probability)) == 0;
    }
    protected static <S extends AbstractSpreadingStructurePlacement> Products.P5<RecordCodecBuilder.Mu<S>, Vec3i, FrequencyReductionMethod, Float, Integer, Optional<ExclusionZone>> placementCodec(RecordCodecBuilder.Instance<S> i) {
        return i.group(Vec3i.offsetCodec(16).optionalFieldOf("locate_offset", Vec3i.ZERO).forGetter(AbstractSpreadingStructurePlacement::locateOffset),
            FrequencyReductionMethod.CODEC.optionalFieldOf("frequency_reduction_method", FrequencyReductionMethod.DEFAULT).forGetter(AbstractSpreadingStructurePlacement::frequencyReductionMethod),
            Codec.floatRange(0.0F, 1.0F).optionalFieldOf("frequency", 1.0F).forGetter(AbstractSpreadingStructurePlacement::frequency),
            net.minecraft.util.ExtraCodecs.NON_NEGATIVE_INT.fieldOf("salt").forGetter(AbstractSpreadingStructurePlacement::salt),
            ExclusionZone.CODEC.optionalFieldOf("exclusion_zone").forGetter(AbstractSpreadingStructurePlacement::exclusionZone));
    }
    protected AbstractSpreadingStructurePlacement(Vec3i locateOffset, FrequencyReductionMethod frequencyReductionMethod, float frequency, int salt, Optional<ExclusionZone> exclusionZone) {
        this.locateOffset = locateOffset;
        this.frequencyReductionMethod = frequencyReductionMethod;
        this.frequency = frequency;
        this.salt = salt;
        this.exclusionZone = exclusionZone;
    }
    public abstract MapCodec<? extends AbstractSpreadingStructurePlacement> codec();
    public Vec3i locateOffset() {
        return locateOffset;
    }
    protected FrequencyReductionMethod frequencyReductionMethod() {
        return frequencyReductionMethod;
    }
    protected float frequency() {
        return frequency;
    }
    protected int salt() {
        return salt;
    }
    protected Optional<ExclusionZone> exclusionZone() {
        return exclusionZone;
    }
    public boolean isStructureChunk(ChunkGeneratorStructureState state, int sourceX, int sourceZ) {
        return isPlacementChunk(state, sourceX, sourceZ) && applyAdditionalChunkRestrictions(sourceX, sourceZ, state.getLevelSeed())
            && applyInteractionsWithOtherStructures(state, sourceX, sourceZ);
    }
    public boolean applyAdditionalChunkRestrictions(int sourceX, int sourceZ, long levelSeed) {
        return !(frequency < 1.0F) || frequencyReductionMethod.shouldGenerate(levelSeed, salt, sourceX, sourceZ, frequency);
    }
    public boolean applyInteractionsWithOtherStructures(ChunkGeneratorStructureState state, int sourceX, int sourceZ) {
        return exclusionZone.isEmpty() || !state.hasStructureChunkInRange(exclusionZone.get().otherSet(), sourceX, sourceZ, exclusionZone.get().chunkCount());
    }
    protected abstract boolean isPlacementChunk(ChunkGeneratorStructureState state, int sourceX, int sourceZ);
    public static record ExclusionZone(Holder<StructureSet> otherSet, int chunkCount) {
        public static final Codec<ExclusionZone> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.placement.AbstractSpreadingStructurePlacement.ExclusionZone.class, "CODEC");
    }
    public static enum FrequencyReductionMethod implements StringRepresentable {
        DEFAULT, LEGACY_TYPE_1, LEGACY_TYPE_2, LEGACY_TYPE_3;
        public static final Codec<FrequencyReductionMethod> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.placement.AbstractSpreadingStructurePlacement.FrequencyReductionMethod.class, "CODEC");
        public boolean shouldGenerate(long seed, int salt, int sourceX, int sourceZ, float probability) {
            return switch (this) {
                case DEFAULT -> defaultReducer(seed, salt, sourceX, sourceZ, probability);
                case LEGACY_TYPE_1 -> outpostReducer(seed, salt, sourceX, sourceZ, probability);
                case LEGACY_TYPE_2 -> fixedSaltReducer(seed, salt, sourceX, sourceZ, probability);
                case LEGACY_TYPE_3 -> doubleReducer(seed, salt, sourceX, sourceZ, probability);
            };
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public interface FrequencyReducer {
        boolean shouldGenerate(long seed, int salt, int sourceX, int sourceZ, float probability);
    }
}
