package net.minecraft.world.level.chunk;

import org.basaltmc.bukkit.Unsupported;
public interface Palette<T> {
    public default int getSize() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.Palette.getSize");
    }
    public default Object valueFor(int index) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.Palette.valueFor");
    }
}
