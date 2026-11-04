// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import net.minecraft.resources.ResourceKey;
import java.util.List;
import java.util.Map;
import net.minecraft.tags.TagKey;

public interface WritableRegistry<T> extends Registry<T> {
    Holder.Reference<T> register(ResourceKey<T> key, T value);
    default Holder.Reference<T> register(ResourceKey<T> key, T value, RegistrationInfo registrationInfo) {
        return register(key, value);
    }
    default void bindTags(Map<TagKey<T>, List<Holder<T>>> pendingTags) {
    }
    default boolean isEmpty() {
        return size() == 0;
    }
    default HolderGetter<T> createRegistrationLookup() {
        return this;
    }
}
