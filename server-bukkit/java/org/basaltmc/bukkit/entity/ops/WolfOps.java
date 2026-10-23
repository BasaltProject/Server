package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.entity.Wolf;

public interface WolfOps extends Wolf, Cored {
    default Wolf.Variant getVariant() {
        String id = core().nbtText("variant");
        Wolf.Variant found = id == null ? null : Registry.WOLF_VARIANT.get(NamespacedKey.fromString(id));
        return found == null ? Wolf.Variant.PALE : found;
    }

    default void setVariant(Wolf.Variant variant) {
        core().nbtSet("variant", "string", variant.getKey().toString());
    }
}
