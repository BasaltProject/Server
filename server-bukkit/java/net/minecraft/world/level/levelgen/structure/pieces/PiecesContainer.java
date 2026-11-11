// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pieces;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.Tag;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;

public record PiecesContainer(List<StructurePiece> pieces) {
    public PiecesContainer(List<StructurePiece> pieces) {
        this.pieces = List.copyOf(pieces);
    }

    public boolean isEmpty() {
        return pieces.isEmpty();
    }

    public boolean isInsidePiece(BlockPos startPos) {
        for (StructurePiece piece : pieces) {
            if (piece.getBoundingBox().isInside(startPos)) {
                return true;
            }
        }
        return false;
    }

    public Tag save(StructurePieceSerializationContext context) {
        ListTag tags = new ListTag();
        for (StructurePiece piece : pieces) {
            tags.add(piece.createTag(context));
        }
        return tags;
    }

    public static PiecesContainer load(ListTag children, StructurePieceSerializationContext context) {
        List<StructurePiece> loaded = new ArrayList<>();
        for (int i = 0; i < children.size(); i++) {
            CompoundTag entry = children.getCompoundOrEmpty(i);
            Identifier id = Identifier.parse(entry.getStringOr("id", "").toLowerCase(Locale.ROOT));
            StructurePieceType type = BuiltInRegistries.STRUCTURE_PIECE.getValue(id);
            if (type == null) {
                continue;
            }
            try {
                loaded.add(type.load(context, entry));
            } catch (RuntimeException broken) {
                continue;
            }
        }
        return new PiecesContainer(loaded);
    }

    public BoundingBox calculateBoundingBox() {
        return StructurePiece.createBoundingBox(pieces.stream());
    }
}
