// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
public abstract class ScatteredFeaturePiece extends StructurePiece {
    protected int width;
    protected int height;
    protected int depth;
    protected int heightPosition = -1;
    protected ScatteredFeaturePiece(StructurePieceType type, int west, int floor, int north, int width, int height, int depth, Direction direction) {
        super(type, 0, StructurePiece.makeBoundingBox(west, floor, north, direction, width, height, depth));
        this.width = width;
        this.height = height;
        this.depth = depth;
        this.setOrientation(direction);
    }
    protected ScatteredFeaturePiece(StructurePieceType type, CompoundTag tag) {
        super(type, tag);
        this.width = tag.getIntOr("Width", 0);
        this.height = tag.getIntOr("Height", 0);
        this.depth = tag.getIntOr("Depth", 0);
        this.heightPosition = tag.getIntOr("HPos", 0);
    }
    protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        tag.putInt("Width", width);
        tag.putInt("Height", height);
        tag.putInt("Depth", depth);
        tag.putInt("HPos", heightPosition);
    }
    protected boolean updateAverageGroundHeight(LevelAccessor level, BoundingBox chunkBB, int offset) {
        if (heightPosition >= 0) {
            return true;
        }
        int sum = 0;
        int samples = 0;
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int z = boundingBox.minZ(); z <= boundingBox.maxZ(); z++) {
            for (int x = boundingBox.minX(); x <= boundingBox.maxX(); x++) {
                cursor.set(x, 64, z);
                if (chunkBB.isInside(cursor)) {
                    sum += level.getHeightmapPos(Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, cursor).getY();
                    samples++;
                }
            }
        }
        if (samples == 0) {
            return false;
        }
        heightPosition = sum / samples;
        boundingBox.move(0, heightPosition - boundingBox.minY() + offset, 0);
        return true;
    }
    protected boolean updateHeightPositionToLowestGroundHeight(LevelAccessor level, int offset) {
        if (heightPosition >= 0) {
            return true;
        }
        int lowest = level.getMaxY() + 1;
        boolean any = false;
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int z = boundingBox.minZ(); z <= boundingBox.maxZ(); z++) {
            for (int x = boundingBox.minX(); x <= boundingBox.maxX(); x++) {
                cursor.set(x, 0, z);
                lowest = Math.min(lowest, level.getHeightmapPos(Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, cursor).getY());
                any = true;
            }
        }
        if (!any) {
            return false;
        }
        heightPosition = lowest;
        boundingBox.move(0, heightPosition - boundingBox.minY() + offset, 0);
        return true;
    }
}
