package net.minecraft.core;

import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import org.basaltmc.bukkit.Unsupported;
public interface Registry<T> extends IdMap, HolderLookup.RegistryLookup {
    public default IdMap asHolderIdMap() {
        throw Unsupported.hit("net.minecraft.core.Registry.asHolderIdMap");
    }
    public default Optional get(Identifier id) {
        throw Unsupported.hit("net.minecraft.core.Registry.get");
    }
    public default Optional get(TagKey id) {
        throw Unsupported.hit("net.minecraft.core.Registry.get");
    }
    public default int getId(Object thing) {
        throw Unsupported.hit("net.minecraft.core.Registry.getId");
    }
    public default Identifier getKey(Object thing) {
        throw Unsupported.hit("net.minecraft.core.Registry.getKey");
    }
    public default Optional getOptional(Identifier key) {
        throw Unsupported.hit("net.minecraft.core.Registry.getOptional");
    }
    public default Holder.Reference getOrThrow(ResourceKey resourceKey) {
        throw Unsupported.hit("net.minecraft.core.Registry.getOrThrow");
    }
    public default Stream getTags() {
        throw Unsupported.hit("net.minecraft.core.Registry.getTags");
    }
    public default Object getValue(Identifier key) {
        throw Unsupported.hit("net.minecraft.core.Registry.getValue");
    }
    public default Object getValueOrThrow(ResourceKey key) {
        throw Unsupported.hit("net.minecraft.core.Registry.getValueOrThrow");
    }
    public default Set keySet() {
        throw Unsupported.hit("net.minecraft.core.Registry.keySet");
    }
    public default Holder wrapAsHolder(Object value) {
        throw Unsupported.hit("net.minecraft.core.Registry.wrapAsHolder");
    }
}
