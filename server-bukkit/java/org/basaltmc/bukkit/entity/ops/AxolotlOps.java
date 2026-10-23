package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Axolotl;

public interface AxolotlOps extends Axolotl, Cored {
    default Axolotl.Variant getVariant() {
        return Axolotl.Variant.values()[Math.floorMod(core().nbtInt("Variant"), Axolotl.Variant.values().length)];
    }

    default void setVariant(Axolotl.Variant variant) {
        core().nbtSet("Variant", "int", variant.ordinal());
    }
}
