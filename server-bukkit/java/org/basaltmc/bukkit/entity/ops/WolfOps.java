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
        long end = core().property("neutral_anger_end") instanceof Number number ? number.longValue() : 0L;
        return end - getWorld().getGameTime() > 0L;
    }

    default void setAngry(boolean angry) {
        core().set("neutral_anger_end", angry ? getWorld().getGameTime() + 600L : -1L);
    }

    default boolean isInterested() {
        return core().flag("wolf_interested");
    }

    default void setInterested(boolean interested) {
        core().set("wolf_interested", interested);
    }

    default boolean isWet() {
        return core().test("wolf_wet");
    }

    default float getTailAngle() {
        if (isAngry()) {
            return 1.5393804f;
        }
        if (isTamed()) {
            double max = getAttribute(org.bukkit.attribute.Attribute.MAX_HEALTH).getValue();
            return (0.55f - (float) ((max - getHealth()) * 0.02)) * (float) Math.PI;
        }
        return 0.62831855f;
    }
}
