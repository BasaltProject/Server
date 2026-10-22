package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public record LongTag(long value) implements NumericTag {
    public long longValue() {
        throw Unsupported.hit("net.minecraft.nbt.LongTag.longValue");
    }
    public static LongTag valueOf(long i) {
        throw Unsupported.hit("net.minecraft.nbt.LongTag.valueOf");
    }
}
