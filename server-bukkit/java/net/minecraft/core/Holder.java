package net.minecraft.core;

import java.util.Optional;
import org.basaltmc.bukkit.Unsupported;
public interface Holder<T> {
    public default Optional unwrapKey() {
        throw Unsupported.hit("net.minecraft.core.Holder.unwrapKey");
    }
    public default Object value() {
        throw Unsupported.hit("net.minecraft.core.Holder.value");
    }

    public static class Reference<T> implements Holder {
        protected Reference() {
        }
        public Object value() {
            throw Unsupported.hit("net.minecraft.core.Holder.Reference.value");
        }
    }
}
