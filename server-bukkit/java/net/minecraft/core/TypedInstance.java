// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.stream.Stream;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
public interface TypedInstance<T> {
    default Holder<T> typeHolder() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.TypedInstance#typeHolder");
    }
    default Stream<TagKey<T>> tags() {
        return typeHolder().tags();
    }
    default boolean is(TagKey<T> tag) {
        return typeHolder().is(tag);
    }
    default boolean is(HolderSet<T> set) {
        return org.basaltmc.bukkit.facade.HolderSets.contains(set, typeHolder());
    }
    default boolean is(T rawType) {
        return typeHolder().value() == rawType;
    }
    default boolean is(Holder<T> type) {
        return typeHolder().is(type);
    }
    default boolean is(ResourceKey<T> type) {
        return typeHolder().is(type);
    }
}
