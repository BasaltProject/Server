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

    default Wolf.SoundVariant getSoundVariant() {
        String id = core().nbtText("sound_variant");
        var registry = io.papermc.paper.registry.RegistryAccess.registryAccess()
            .getRegistry(io.papermc.paper.registry.RegistryKey.WOLF_SOUND_VARIANT);
        Wolf.SoundVariant found = id == null ? null : registry.get(NamespacedKey.fromString(id));
        return found != null ? found : Wolf.SoundVariant.CLASSIC;
    }

    default void setSoundVariant(Wolf.SoundVariant variant) {
        EntityNbt.variant(core(), "sound_variant", variant);
    }

    default boolean isAngry() {
        long end = core().nbtValue("anger_end_time") instanceof Number number ? number.longValue() : 0L;
        return end - getWorld().getGameTime() > 0L;
    }

    default void setAngry(boolean angry) {
        core().nbtSet("anger_end_time", "long", angry ? getWorld().getGameTime() + 600L : 0L);
    }
}
