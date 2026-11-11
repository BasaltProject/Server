// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.ChunkPos;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class EndCityStructure extends Structure {
    public static final MapCodec<EndCityStructure> CODEC = simpleCodec(EndCityStructure::new);
    public EndCityStructure(Structure.StructureSettings settings) {
        super(settings);
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        ChunkPos chunk = context.chunkPos();
        int x = chunk.getBlockX(7);
        int z = chunk.getBlockZ(7);
        if (!context.couldValidBiomeExistInTerrainColumn(x, z)) {
            return Optional.empty();
        }
        Rotation rotation = Rotation.getRandom(context.random());
        BlockPos start = getLowestYIn5by5Box(context, x, z, rotation);
        if (start.getY() < 60) {
            return Optional.empty();
        }
        return Optional.of(new Structure.GenerationStub(start, builder -> {
            List<StructurePiece> pieces = new ArrayList<>();
            EndCityPieces.startHouseTower(context.structureTemplateManager(), start, rotation, pieces, context.random());
            pieces.forEach(builder::addPiece);
        }));
    }
    public StructureType<?> type() {
        return StructureType.END_CITY;


    }
}
