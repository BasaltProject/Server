package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public record FloatTag(float value) implements NumericTag {
    public float floatValue() {
        throw Unsupported.hit("net.minecraft.nbt.FloatTag.floatValue");
    }
    public static FloatTag valueOf(float data) {
        throw Unsupported.hit("net.minecraft.nbt.FloatTag.valueOf");
    }
}
