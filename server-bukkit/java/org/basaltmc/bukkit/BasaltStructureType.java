package org.basaltmc.bukkit;

import org.bukkit.NamespacedKey;
import org.bukkit.generator.structure.StructureType;

/** A kind of structure by the name the game gives it. */
final class BasaltStructureType extends StructureType {
    private final NamespacedKey key;

    BasaltStructureType(NamespacedKey key) {
        this.key = key;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public String toString() {
        return "StructureType{" + key + "}";
    }
}
