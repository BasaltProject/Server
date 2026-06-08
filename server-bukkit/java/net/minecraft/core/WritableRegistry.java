package net.minecraft.core;

import java.util.stream.Stream;
import org.basaltmc.bukkit.Unsupported;
public interface WritableRegistry<T> extends Registry {
    public default Stream stream() {
        throw Unsupported.hit("net.minecraft.core.WritableRegistry.stream");
    }
}
