package net.minecraft.core;

import java.util.Iterator;
import org.basaltmc.bukkit.Unsupported;
public interface DefaultedRegistry<T> extends Registry {
    public default Iterator iterator() {
        throw Unsupported.hit("net.minecraft.core.DefaultedRegistry.iterator");
    }
}
