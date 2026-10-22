package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public record StringTag(String value) implements PrimitiveTag {
    public static StringTag valueOf(String data) {
        throw Unsupported.hit("net.minecraft.nbt.StringTag.valueOf");
    }
}
