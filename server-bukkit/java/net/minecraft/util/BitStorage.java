package net.minecraft.util;

import org.basaltmc.bukkit.Unsupported;
public interface BitStorage {
    public default int getBits() {
        throw Unsupported.hit("net.minecraft.util.BitStorage.getBits");
    }
    public default long[] getRaw() {
        throw Unsupported.hit("net.minecraft.util.BitStorage.getRaw");
    }
}
