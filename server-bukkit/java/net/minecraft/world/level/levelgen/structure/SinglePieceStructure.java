// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import java.util.Optional;
import net.minecraft.world.level.levelgen.WorldgenRandom;
public abstract class SinglePieceStructure extends Structure {
    private final PieceConstructor constructor;
    private final int width;
    private final int depth;
    protected SinglePieceStructure(PieceConstructor constructor, int width, int depth, Structure.StructureSettings settings) {
        super(settings);
        this.constructor = constructor;
        this.width = width;
        this.depth = depth;
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        if (!context.couldValidBiomeExistOnTopOfChunkCenter() || getLowestY(context, width, depth) < context.chunkGenerator().getSeaLevel()) {
            return Optional.empty();
        }
        return onTopOfChunkCenterWithoutBiomeCheck(context, Heightmap.Types.WORLD_SURFACE_WG,
            builder -> builder.addPiece(constructor.construct(context.random(), context.chunkPos().getMinBlockX(), context.chunkPos().getMinBlockZ())));
    }
    protected interface PieceConstructor {
        StructurePiece construct(WorldgenRandom random, int x, int z);
    }
}
