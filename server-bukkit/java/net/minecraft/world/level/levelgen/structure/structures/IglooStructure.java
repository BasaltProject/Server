// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Rotation;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class IglooStructure extends Structure {
    public static final MapCodec<IglooStructure> CODEC = simpleCodec(IglooStructure::new);
    public IglooStructure(Structure.StructureSettings settings) {
        super(settings);
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return onTopOfChunkCenter(context, Heightmap.Types.WORLD_SURFACE_WG, builder -> {
            BlockPos start = new BlockPos(context.chunkPos().getMinBlockX(), 90, context.chunkPos().getMinBlockZ());
            Rotation rotation = Rotation.getRandom(context.random());
            IglooPieces.addPieces(context.structureTemplateManager(), start, rotation, builder, context.random());
        });
    }
    public StructureType<?> type() {
        return StructureType.IGLOO;
    }
}
