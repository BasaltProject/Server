package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Pig;

public interface PigOps extends Pig, Cored {
    default Pig.Variant getVariant() {
        return EntityNbt.variant(core(), "variant", io.papermc.paper.registry.RegistryKey.PIG_VARIANT, "temperate");
    }

    default void setVariant(Pig.Variant variant) {
        EntityNbt.variant(core(), "variant", variant);
    }

    default Pig.SoundVariant getSoundVariant() {
        return EntityNbt.variant(core(), "sound_variant", io.papermc.paper.registry.RegistryKey.PIG_SOUND_VARIANT,
            "classic");
    }

    default void setSoundVariant(Pig.SoundVariant variant) {
        EntityNbt.variant(core(), "sound_variant", variant);
    }
}
