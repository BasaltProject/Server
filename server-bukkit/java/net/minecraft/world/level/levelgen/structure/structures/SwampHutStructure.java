// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class SwampHutStructure extends Structure {
    public static final MapCodec<SwampHutStructure> CODEC = simpleCodec(SwampHutStructure::new);
    public SwampHutStructure(Structure.StructureSettings settings) {
        super(settings);
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return onTopOfChunkCenter(context, Heightmap.Types.WORLD_SURFACE_WG,
            builder -> builder.addPiece(new SwampHutPiece(context.random(), context.chunkPos().getMinBlockX(), context.chunkPos().getMinBlockZ())));
    }
    public StructureType<?> type() {
        return StructureType.SWAMP_HUT;
    }
}
