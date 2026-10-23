package org.basaltmc.bukkit.persistence;

import net.minecraft.nbt.CompoundTag;

public final class MemoryPdcStore implements PdcStore {
    private CompoundTag tag;

    public MemoryPdcStore() {
        this(new CompoundTag());
    }

    public MemoryPdcStore(CompoundTag tag) {
        this.tag = tag;
    }

    @Override
    public CompoundTag load() {
        return tag;
    }

    @Override
    public void save(CompoundTag next) {
        tag = next;
    }
}
