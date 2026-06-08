package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public record ByteTag(byte value) implements NumericTag {
    public byte byteValue() {
        throw Unsupported.hit("net.minecraft.nbt.ByteTag.byteValue");
    }
    public static ByteTag valueOf(byte data) {
        throw Unsupported.hit("net.minecraft.nbt.ByteTag.valueOf");
    }
}
