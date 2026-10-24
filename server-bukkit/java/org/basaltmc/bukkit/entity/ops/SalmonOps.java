package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Salmon;

public interface SalmonOps extends Salmon, Cored {
    default Salmon.Variant getVariant() {
        String name = core().nbtText("type");
        return name == null ? Salmon.Variant.MEDIUM : Salmon.Variant.valueOf(name.toUpperCase(java.util.Locale.ROOT));
    }

    default void setVariant(Salmon.Variant variant) {
        core().nbtSet("type", "string", variant.name().toLowerCase(java.util.Locale.ROOT));
    }
}
