package org.basaltmc.bukkit;

import org.bukkit.NamespacedKey;
import org.bukkit.block.Biome;

public final class BasaltBiome implements Biome {
    private final NamespacedKey key;
    private final int ordinal;

    BasaltBiome(String id, int ordinal) {
        this.key = NamespacedKey.fromString(id);
        this.ordinal = ordinal;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public String name() {
        return key.getKey().toUpperCase(java.util.Locale.ROOT);
    }

    @Override
    public int ordinal() {
        return ordinal;
    }

    @Override
    public int compareTo(Biome other) {
        return Integer.compare(ordinal, other.ordinal());
    }

    @Override
    public String translationKey() {
        return "biome." + key.getNamespace() + "." + key.getKey();
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltBiome biome && biome.key.equals(key);
    }

    @Override
    public int hashCode() {
        return key.hashCode();
    }

    @Override
    public String toString() {
        return key.toString();
    }
}
