package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.entity.Cat;

public interface CatOps extends Cat, Cored {
    default Cat.Type getCatType() {
        String id = core().nbtText("variant");
        Cat.Type found = id == null ? null : Registry.CAT_VARIANT.get(NamespacedKey.fromString(id));
        return found == null ? Cat.Type.TABBY : found;
    }

    default void setCatType(Cat.Type type) {
        core().nbtSet("variant", "string", type.getKey().toString());
    }

    default Cat.SoundVariant getSoundVariant() {
        return EntityNbt.variant(core(), "sound_variant", io.papermc.paper.registry.RegistryKey.CAT_SOUND_VARIANT,
            "classic");
    }

    default void setSoundVariant(Cat.SoundVariant variant) {
        EntityNbt.variant(core(), "sound_variant", variant);
    }
}
