package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public record ShortTag(short value) implements NumericTag {
    public short shortValue() {
        throw Unsupported.hit("net.minecraft.nbt.ShortTag.shortValue");
    }
    public static ShortTag valueOf(short i) {
        throw Unsupported.hit("net.minecraft.nbt.ShortTag.valueOf");
    }
}
