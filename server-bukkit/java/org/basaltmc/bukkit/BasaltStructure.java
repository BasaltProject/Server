package org.basaltmc.bukkit;

import org.bukkit.NamespacedKey;
import org.bukkit.generator.structure.Structure;
import org.bukkit.generator.structure.StructureType;

/** A structure by the name the game gives it. */
final class BasaltStructure extends Structure {
    private final NamespacedKey key;

    BasaltStructure(NamespacedKey key) {
        this.key = key;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public StructureType getStructureType() {
        throw Unsupported.hit("Structure#getStructureType");
    }

    @Override
    public String toString() {
        return "Structure{" + key + "}";
    }
}
