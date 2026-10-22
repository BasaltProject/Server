package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public class LongArrayTag implements CollectionTag {
    public LongArrayTag(long[] values) {
    }
    public long[] getAsLongArray() {
        throw Unsupported.hit("net.minecraft.nbt.LongArrayTag.getAsLongArray");
    }
}
