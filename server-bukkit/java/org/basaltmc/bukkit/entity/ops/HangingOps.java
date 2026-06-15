package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.Hanging;

public interface HangingOps extends Hanging, Cored {
    private String key() {
        return core().type.getKey().getKey().equals("painting") ? "facing" : "Facing";
    }

    private boolean painting() {
        return core().type.getKey().getKey().equals("painting");
    }

    default BlockFace getAttachedFace() {
        return getFacing().getOppositeFace();
    }

    default boolean setFacingDirection(BlockFace face, boolean force) {
        int id;
        if (painting()) {
            id = switch (face) {
                case SOUTH -> 0;
                case WEST -> 1;
                case NORTH -> 2;
                case EAST -> 3;
                default -> -1;
            };
        } else {
            id = switch (face) {
                case DOWN -> 0;
                case UP -> 1;
                case NORTH -> 2;
                case SOUTH -> 3;
                case WEST -> 4;
                case EAST -> 5;
                default -> -1;
            };
        }
        if (id < 0) {
            return false;
        }
        core().nbtSet(key(), "byte", id);
        return true;
    }

    default void setFacingDirection(BlockFace face) {
        setFacingDirection(face, false);
    }
}
