// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.ScatteredFeaturePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
public class DesertPyramidPiece extends ScatteredFeaturePiece {
    private final boolean[] hasPlacedChest = new boolean[4];
    private final List<BlockPos> potentialSuspiciousSandWorldPositions = new ArrayList<>();
    private BlockPos randomCollapsedRoofPos = BlockPos.ZERO;
    public static final int WIDTH = 21;
    public static final int DEPTH = 21;
    public DesertPyramidPiece(RandomSource random, int west, int north) {
        super(StructurePieceType.DESERT_PYRAMID_PIECE, west, 64, north, 21, 15, 21, getRandomHorizontalDirection(random));
    }
    public DesertPyramidPiece(CompoundTag tag) {
        super(StructurePieceType.DESERT_PYRAMID_PIECE, tag);
        for (int i = 0; i < hasPlacedChest.length; i++) {
            hasPlacedChest[i] = tag.getBooleanOr("hasPlacedChest" + i, false);
        }
    }
    protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        super.addAdditionalSaveData(context, tag);
        for (int i = 0; i < hasPlacedChest.length; i++) {
            tag.putBoolean("hasPlacedChest" + i, hasPlacedChest[i]);
        }
    }
    public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
        return;
    }
    public List<BlockPos> getPotentialSuspiciousSandWorldPositions() {
        return potentialSuspiciousSandWorldPositions;
    }
    public BlockPos getRandomCollapsedRoofPos() {
        return randomCollapsedRoofPos;
    }
}
