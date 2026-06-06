package org.basaltmc.bukkit.worldedit.fawe;

import com.fastasyncworldedit.core.nbt.FaweCompoundTag;
import org.basaltmc.bukkit.worldedit.BasaltBlockMaterial;

/** The block facts FastAsyncWorldEdit asks for on top of the common ones. */
final class BasaltFaweBlockMaterial extends BasaltBlockMaterial {
    BasaltFaweBlockMaterial(String id, int defaultState) {
        super(id, defaultState);
    }

    @Override
    public boolean hasContainer() {
        return isTile();
    }

    public FaweCompoundTag defaultTile() {
        return null;
    }
}
