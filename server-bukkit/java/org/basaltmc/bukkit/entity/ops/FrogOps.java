package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.entity.Frog;

public interface FrogOps extends Frog, Cored {
    default Frog.Variant getVariant() {
        String id = core().nbtText("variant");
        Frog.Variant found = id == null ? null : Registry.FROG_VARIANT.get(NamespacedKey.fromString(id));
        return found == null ? Frog.Variant.TEMPERATE : found;
    }

    default void setVariant(Frog.Variant variant) {
        core().nbtSet("variant", "string", variant.getKey().toString());
    }
}
