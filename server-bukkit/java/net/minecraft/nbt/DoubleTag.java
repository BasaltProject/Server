package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public record DoubleTag(double value) implements NumericTag {
    public double doubleValue() {
        throw Unsupported.hit("net.minecraft.nbt.DoubleTag.doubleValue");
    }
    public static DoubleTag valueOf(double data) {
        throw Unsupported.hit("net.minecraft.nbt.DoubleTag.valueOf");
    }
}
