package org.basaltmc.bukkit.persistence;

import net.minecraft.nbt.CompoundTag;

/** Where a persistent data container keeps its compound; every holder supplies one. */
public interface PdcStore {
    /** The current compound, never null; a caller that changes it hands it back through {@link #save}. */
    CompoundTag load();

    void save(CompoundTag tag);
}
