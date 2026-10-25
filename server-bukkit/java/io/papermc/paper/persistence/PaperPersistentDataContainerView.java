package io.papermc.paper.persistence;

import java.io.IOException;
import java.util.LinkedHashSet;
import java.util.Objects;
import java.util.Set;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.Tag;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.basaltmc.bukkit.persistence.PdcStore;
import org.basaltmc.bukkit.persistence.PdcTags;
import org.bukkit.NamespacedKey;
import org.bukkit.craftbukkit.persistence.CraftPersistentDataContainer;
import org.bukkit.persistence.PersistentDataAdapterContext;
import org.bukkit.persistence.PersistentDataContainer;
import org.bukkit.persistence.PersistentDataType;

public abstract class PaperPersistentDataContainerView implements PersistentDataContainerView {
    private static final PersistentDataAdapterContext kContext = CraftPersistentDataContainer::newContainer;

    protected final PdcStore store;

    protected PaperPersistentDataContainerView(PdcStore store) {
        this.store = Objects.requireNonNull(store, "store");
    }

    @Override
    public <P, C> boolean has(NamespacedKey key, PersistentDataType<P, C> type) {
        Objects.requireNonNull(key, "key");
        Objects.requireNonNull(type, "type");
        Tag tag = read().get(key.toString());
        return tag != null && PdcTags.matches(type, tag);
    }

    @Override
    public boolean has(NamespacedKey key) {
        Objects.requireNonNull(key, "key");
        return read().contains(key.toString());
    }

    @Override
    @SuppressWarnings("unchecked")
    public <P, C> C get(NamespacedKey key, PersistentDataType<P, C> type) {
        Objects.requireNonNull(key, "key");
        Objects.requireNonNull(type, "type");
        Tag tag = read().get(key.toString());
        return tag == null ? null : (C) PdcTags.unwrap(type, tag, kContext);
    }

    @Override
    public <P, C> C getOrDefault(NamespacedKey key, PersistentDataType<P, C> type, C defaultValue) {
        C found = get(key, type);
        return found != null ? found : defaultValue;
    }

    @Override
    public Set<NamespacedKey> getKeys() {
        Set<NamespacedKey> keys = new LinkedHashSet<>();
        for (String name : read().keySet()) {
            NamespacedKey key = NamespacedKey.fromString(name);
            if (key != null) {
                keys.add(key);
            }
        }
        return keys;
    }

    @Override
    public boolean isEmpty() {
        return read().isEmpty();
    }

    @Override
    public void copyTo(PersistentDataContainer other, boolean replace) {
        Objects.requireNonNull(other, "other");
        CompoundTag mine = read();
        if (other instanceof CraftPersistentDataContainer craft) {
            craft.putAll(mine, replace);
            return;
        }
        throw new IllegalArgumentException("Cannot copy into a foreign container");
    }

    @Override
    public PersistentDataAdapterContext getAdapterContext() {
        return kContext;
    }

    @Override
    public byte[] serializeToBytes() throws IOException {
        return NbtCodec.toBytes(read());
    }

    @Override
    public int getSize() {
        return read().size();
    }

    protected final CompoundTag read() {
        return store.load();
    }

    public CompoundTag toTagCompound() {
        return read().copy();
    }

    public CompoundTag getRaw() {
        return read();
    }

    public Tag getTag(String key) {
        return read().get(key);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof PaperPersistentDataContainerView view && view.read().equals(read());
    }

    @Override
    public int hashCode() {
        return read().hashCode();
    }

    @Override
    public String toString() {
        return read().toString();
    }
}
