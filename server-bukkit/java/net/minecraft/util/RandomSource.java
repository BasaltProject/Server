package net.minecraft.util;

import org.basaltmc.bukkit.Unsupported;
public interface RandomSource {
    static RandomSource create() {
        throw Unsupported.hit("net.minecraft.util.RandomSource.create");
    }
}
