package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.MushroomCow;

public interface MushroomCowOps extends MushroomCow, Cored {
    default MushroomCow.Variant getVariant() {
        return "brown".equals(core().nbtText("Type")) ? MushroomCow.Variant.BROWN : MushroomCow.Variant.RED;
    }

    default void setVariant(MushroomCow.Variant variant) {
        core().nbtSet("Type", "string", variant == MushroomCow.Variant.BROWN ? "brown" : "red");
    }
}
