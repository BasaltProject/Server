package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.Shulker;

public interface ShulkerOps extends Shulker, Cored {
    BlockFace[] kFaces = {BlockFace.DOWN, BlockFace.UP, BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST,
        BlockFace.EAST};

    default float getPeek() {
        return core().integer("shulker_peek") / 100.0F;
    }

    default void setPeek(float peek) {
        if (peek < 0.0F || peek > 1.0F) {
            throw new IllegalArgumentException("Peek must be between 0.0 and 1.0");
        }
        core().call("shulker_set_peek", (long) Math.round(peek * 100.0F));
    }

    default BlockFace getAttachedFace() {
        return kFaces[Math.max(0, Math.min(5, core().integer("shulker_face")))];
    }

    default void setAttachedFace(BlockFace face) {
        for (int at = 0; at < kFaces.length; at++) {
            if (kFaces[at] == face) {
                core().set("shulker_face", at);
                return;
            }
        }
        throw new IllegalArgumentException("A shulker attaches to a block face, not " + face);
    }
}
