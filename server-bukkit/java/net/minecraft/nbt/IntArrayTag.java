package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public class IntArrayTag implements CollectionTag {
    public IntArrayTag(int[] values) {
    }
    public int[] getAsIntArray() {
        throw Unsupported.hit("net.minecraft.nbt.IntArrayTag.getAsIntArray");
    }
}
