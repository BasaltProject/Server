package org.basaltmc.bukkit.persistence;

import net.minecraft.nbt.CompoundTag;

public interface PdcStore {
    /** The current compound, never null; a caller that changes it hands it back through {@link #save}. */
    CompoundTag load();

    void save(CompoundTag tag);
}
