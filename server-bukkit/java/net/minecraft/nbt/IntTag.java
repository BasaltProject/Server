package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public record IntTag(int value) implements NumericTag {
    public int intValue() {
        throw Unsupported.hit("net.minecraft.nbt.IntTag.intValue");
    }
    public static IntTag valueOf(int i) {
        throw Unsupported.hit("net.minecraft.nbt.IntTag.valueOf");
    }
}
