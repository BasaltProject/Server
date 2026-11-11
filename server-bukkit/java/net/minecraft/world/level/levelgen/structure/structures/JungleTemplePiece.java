// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.ScatteredFeaturePiece;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
public class JungleTemplePiece extends ScatteredFeaturePiece {
    private boolean placedMainChest;
    private boolean placedHiddenChest;
    private boolean placedTrap1;
    private boolean placedTrap2;
    public static final int WIDTH = 12;
    public static final int DEPTH = 15;
    public JungleTemplePiece(RandomSource random, int west, int north) {
        super(StructurePieceType.JUNGLE_PYRAMID_PIECE, west, 64, north, 12, 10, 15, getRandomHorizontalDirection(random));
    }
    public JungleTemplePiece(CompoundTag tag) {
        super(StructurePieceType.JUNGLE_PYRAMID_PIECE, tag);
        placedMainChest = tag.getBooleanOr("placedMainChest", false);
        placedHiddenChest = tag.getBooleanOr("placedHiddenChest", false);
        placedTrap1 = tag.getBooleanOr("placedTrap1", false);
        placedTrap2 = tag.getBooleanOr("placedTrap2", false);
    }
    protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        super.addAdditionalSaveData(context, tag);
        tag.putBoolean("placedMainChest", placedMainChest);
        tag.putBoolean("placedHiddenChest", placedHiddenChest);
        tag.putBoolean("placedTrap1", placedTrap1);
        tag.putBoolean("placedTrap2", placedTrap2);
    }
    public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
        return;
    }
    static class MossStoneSelector extends StructurePiece.BlockSelector {
        public void next(RandomSource random, int worldX, int worldY, int worldZ, boolean isEdge) {
            this.next = (random.nextFloat() < 0.4F ? Blocks.COBBLESTONE : Blocks.MOSSY_COBBLESTONE).defaultBlockState();


        }
    }
}
