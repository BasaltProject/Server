package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Cow;

public interface CowOps extends Cow, Cored {
    default Cow.Variant getVariant() {
        return EntityNbt.variant(core(), "variant", io.papermc.paper.registry.RegistryKey.COW_VARIANT, "temperate");
    }

    default void setVariant(Cow.Variant variant) {
        EntityNbt.variant(core(), "variant", variant);
    }

    default Cow.SoundVariant getSoundVariant() {
        return EntityNbt.variant(core(), "sound_variant", io.papermc.paper.registry.RegistryKey.COW_SOUND_VARIANT,
            "classic");
    }

    default void setSoundVariant(Cow.SoundVariant variant) {
        EntityNbt.variant(core(), "sound_variant", variant);
    }
}
