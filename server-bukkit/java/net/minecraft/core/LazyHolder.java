// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.function.Supplier;
import net.minecraft.resources.ResourceKey;

/** A reference holder whose value is built from native data on first use. */
public final class LazyHolder<T> extends Holder.Reference<T> {
    private final Supplier<T> loader;
    private volatile T loaded;
    private volatile boolean done;

    public LazyHolder(ResourceKey<T> key, Supplier<T> loader) {
        super(Type.STAND_ALONE, null, key, null);
        this.loader = loader;
    }

    @Override
    public T value() {
        if (!done) {
            synchronized (this) {
                if (!done) {
                    loaded = loader.get();
                    done = true;
                }
            }
        }
        return loaded;
    }

    @Override
    public boolean isBound() {
        return value() != null;
    }

    @Override
    public boolean is(Holder<T> other) {
        return other == this || other.unwrapKey().equals(unwrapKey());
    }

    @Override
    public String toString() {
        return "LazyReference{" + key() + "}";
    }
}
