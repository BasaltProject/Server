// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.structures;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtUtils;

public class StructureUpdater implements SnbtToNbt.Filter {
    @Override
    public CompoundTag apply(String name, CompoundTag input) {
        return update(name, input);
    }

    public static CompoundTag update(String name, CompoundTag tag) {
        return NbtUtils.addCurrentDataVersion(tag);
    }
}
