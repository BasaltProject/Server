// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class BuriedTreasureStructure extends Structure {
    public static final MapCodec<BuriedTreasureStructure> CODEC = simpleCodec(BuriedTreasureStructure::new);
    public BuriedTreasureStructure(Structure.StructureSettings settings) {
        super(settings);
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return onTopOfChunkCenter(context, Heightmap.Types.OCEAN_FLOOR_WG,
            builder -> builder.addPiece(new BuriedTreasurePieces.BuriedTreasurePiece(new BlockPos(context.chunkPos().getBlockX(9), 90, context.chunkPos().getBlockZ(9)))));
    }
    public StructureType<?> type() {
        return StructureType.BURIED_TREASURE;
    }
}
