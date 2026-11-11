// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.registries;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderOwner;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
public class ConcurrentHolderGetter<T> implements HolderGetter<T> {
    private final Object guard;
    private final HolderGetter<T> source;
    private final Map<ResourceKey<T>, Optional<Holder.Reference<T>>> byKey = new ConcurrentHashMap<>();
    private final Map<TagKey<T>, Optional<HolderSet.Named<T>>> byTag = new ConcurrentHashMap<>();
    public ConcurrentHolderGetter(Object lock, HolderGetter<T> original) {
        this.guard = lock;
        this.source = original;
    }
    public Optional<Holder.Reference<T>> get(ResourceKey<T> elementId) {
        return byKey.computeIfAbsent(elementId, k -> {
            synchronized (guard) {
                return source.get(k);
            }
        });
    }
    public Optional<HolderSet.Named<T>> get(TagKey<T> tagId) {
        return byTag.computeIfAbsent(tagId, k -> {
            synchronized (guard) {
                return source.get(k);
            }
        });
    }
    public boolean canSerialize(HolderOwner<T> owner) {
        return source.canSerializeIn(owner);
    }
}
