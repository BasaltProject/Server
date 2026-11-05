// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;

import java.util.List;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.pieces.PiecesContainer;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;

public class StructureStart {
    public static final String INVALID_START_ID = "INVALID";
    public static final StructureStart INVALID_START = new StructureStart();
    private final Structure structure;
    private final ChunkPos chunkPos;
    private final BoundingBox box;
    private int references;
    private final List<StructurePiece> pieces;

    protected StructureStart() {
        this.structure = null;
        this.chunkPos = null;
        this.box = null;
        this.pieces = List.of();
    }

    public StructureStart(Structure structure, ChunkPos chunkPos, int references, PiecesContainer pieceContainer) {
        this(structure, chunkPos, references, pieceContainer.pieces().isEmpty()
            ? new BoundingBox(chunkPos.getMinBlockX(), 0, chunkPos.getMinBlockZ(), chunkPos.getMaxBlockX(), 0, chunkPos.getMaxBlockZ())
            : StructurePiece.createBoundingBox(pieceContainer.pieces().stream()), pieceContainer.pieces());
    }

    public StructureStart(Structure structure, ChunkPos chunkPos, int references, BoundingBox box) {
        this(structure, chunkPos, references, box, List.of());
    }

    public StructureStart(Structure structure, ChunkPos chunkPos, int references, BoundingBox box, List<StructurePiece> pieces) {
        this.structure = structure;
        this.chunkPos = chunkPos;
        this.references = references;
        this.box = box;
        this.pieces = List.copyOf(pieces);
    }

    public BoundingBox getBoundingBox() {
        return box;
    }

    public boolean isValid() {
        return box != null;
    }

    public void placeInChunk(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos) {
    }

    public static StructureStart loadStaticStart(StructurePieceSerializationContext context, CompoundTag tag, long seed) {
        return INVALID_START;
    }

    public CompoundTag createTag(StructurePieceSerializationContext context, ChunkPos chunkPos) {
        CompoundTag tag = new CompoundTag();
        if (isValid()) {
            tag.putString("id", String.valueOf(structure));
            tag.putInt("ChunkX", chunkPos.x());
            tag.putInt("ChunkZ", chunkPos.z());
            tag.putInt("references", references);
        } else {
            tag.putString("id", INVALID_START_ID);
        }
        return tag;
    }

    public ChunkPos getChunkPos() {
        return chunkPos;
    }

    public boolean canBeReferenced() {
        return references < getMaxReferences();
    }

    public void addReference() {
        references++;
    }

    public int getReferences() {
        return references;
    }

    protected int getMaxReferences() {
        return 1;
    }

    public Structure getStructure() {
        return structure;
    }

    public List<StructurePiece> getPieces() {
        return pieces;
    }
}
