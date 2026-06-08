package net.minecraft.core;

import org.basaltmc.bukkit.Unsupported;
public interface IdMap<T> {
    public default Object byId(int id) {
        throw Unsupported.hit("net.minecraft.core.IdMap.byId");
    }
    public default Object byIdOrThrow(int id) {
        throw Unsupported.hit("net.minecraft.core.IdMap.byIdOrThrow");
    }
    public default int size() {
        throw Unsupported.hit("net.minecraft.core.IdMap.size");
    }
}
