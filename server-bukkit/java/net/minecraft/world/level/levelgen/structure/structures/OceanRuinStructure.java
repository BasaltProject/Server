// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Rotation;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class OceanRuinStructure extends Structure {
    public static final MapCodec<OceanRuinStructure> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(settingsCodec(i),
        Type.CODEC.fieldOf("biome_temp").forGetter(c -> c.biomeTemp), Codec.floatRange(0.0F, 1.0F).fieldOf("large_probability").forGetter(c -> c.largeProbability),
        Codec.floatRange(0.0F, 1.0F).fieldOf("cluster_probability").forGetter(c -> c.clusterProbability)).apply(i, OceanRuinStructure::new));
    public final Type biomeTemp;
    public final float largeProbability;
    public final float clusterProbability;
    public OceanRuinStructure(Structure.StructureSettings settings, Type biomeTemp, float largeProbability, float clusterProbability) {
        super(settings);
        this.biomeTemp = biomeTemp;
        this.largeProbability = largeProbability;
        this.clusterProbability = clusterProbability;
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return onTopOfChunkCenter(context, Heightmap.Types.OCEAN_FLOOR_WG, builder -> {
            BlockPos at = new BlockPos(context.chunkPos().getMinBlockX(), 90, context.chunkPos().getMinBlockZ());
            Rotation turn = Rotation.getRandom(context.random());
            OceanRuinPieces.addPieces(context.structureTemplateManager(), at, turn, builder, context.random(), this);
        });
    }
    public StructureType<?> type() {
        return StructureType.OCEAN_RUIN;
    }
    public static enum Type implements StringRepresentable {
        WARM, COLD;
        public static final Codec<Type> CODEC = StringRepresentable.fromEnum(Type::values);
        public static final Codec<Type> LEGACY_CODEC = net.minecraft.util.ExtraCodecs.legacyEnum(Type::valueOf);
        public String getName() {
            return getSerializedName();
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
