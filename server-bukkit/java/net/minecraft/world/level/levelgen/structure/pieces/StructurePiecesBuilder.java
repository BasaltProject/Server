// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pieces;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;

public class StructurePiecesBuilder {
    private final List<StructurePiece> pieces = new ArrayList<>();

    public void addPiece(StructurePiece piece) {
        pieces.add(piece);
    }

    public StructurePiece findCollisionPiece(BoundingBox box) {
        return StructurePiece.findCollisionPiece(pieces, box);
    }

    @Deprecated
    public void offsetPiecesVertically(int dy) {
        for (StructurePiece piece : pieces) {
            piece.move(0, dy, 0);
        }
    }

    @Deprecated
    public int moveBelowSeaLevel(int seaLevel, int minY, RandomSource random, int offset) {
        int ceiling = seaLevel - offset;
        BoundingBox box = getBoundingBox();
        int target = box.getYSpan() + minY + 1;
        if (target < ceiling) {
            target += random.nextInt(ceiling - target);
        }
        int shift = target - box.maxY();
        offsetPiecesVertically(shift);
        return shift;
    }

    public void moveInsideHeights(RandomSource random, int lowestAllowed, int highestAllowed) {
        BoundingBox box = getBoundingBox();
        int room = highestAllowed - lowestAllowed + 1 - box.getYSpan();
        int bottom = room > 1 ? lowestAllowed + random.nextInt(room) : lowestAllowed;
        offsetPiecesVertically(bottom - box.minY());
    }

    public PiecesContainer build() {
        return new PiecesContainer(pieces);
    }

    public void clear() {
        pieces.clear();
    }

    public boolean isEmpty() {
        return pieces.isEmpty();
    }

    public BoundingBox getBoundingBox() {
        return StructurePiece.createBoundingBox(pieces.stream());
    }
}
