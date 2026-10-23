package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Parrot;

public interface ParrotOps extends Parrot, Cored {
    default Parrot.Variant getVariant() {
        return Parrot.Variant.values()[Math.floorMod(core().nbtInt("Variant"), Parrot.Variant.values().length)];
    }

    default void setVariant(Parrot.Variant variant) {
        core().nbtSet("Variant", "int", variant.ordinal());
    }
}
