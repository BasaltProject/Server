// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class ShipwreckStructure extends Structure {
    public static final MapCodec<ShipwreckStructure> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(settingsCodec(i),
        com.mojang.serialization.Codec.BOOL.fieldOf("is_beached").forGetter(s -> s.isBeached)).apply(i, ShipwreckStructure::new));
    public final boolean isBeached;
    public ShipwreckStructure(Structure.StructureSettings settings, boolean isBeached) {
        super(settings);
        this.isBeached = isBeached;
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        Heightmap.Types map = isBeached ? Heightmap.Types.WORLD_SURFACE_WG : Heightmap.Types.OCEAN_FLOOR_WG;
        return onTopOfChunkCenter(context, map, builder -> {
            Rotation rotation = Rotation.getRandom(context.random());
            BlockPos start = new BlockPos(context.chunkPos().getMinBlockX(), 90, context.chunkPos().getMinBlockZ());
            ShipwreckPieces.ShipwreckPiece piece = ShipwreckPieces.addRandomPiece(context.structureTemplateManager(), start, rotation, builder, context.random(), isBeached);
            if (!piece.isTooBigToFitInWorldGenRegion()) {
                return;
            }
            BoundingBox box = piece.getBoundingBox();
            int y = isBeached
                ? piece.calculateBeachedPosition(Structure.getLowestY(context, box.minX(), box.getXSpan(), box.minZ(), box.getZSpan()), context.random())
                : Structure.getMeanFirstOccupiedHeight(context, box.minX(), box.getXSpan(), box.minZ(), box.getZSpan());
            piece.adjustPositionHeight(y);
        });
    }
    public StructureType<?> type() {
        return StructureType.SHIPWRECK;
    }
}
