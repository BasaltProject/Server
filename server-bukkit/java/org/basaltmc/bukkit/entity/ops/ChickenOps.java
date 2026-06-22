package org.basaltmc.bukkit.entity.ops;


import io.papermc.paper.registry.RegistryAccess;
import io.papermc.paper.registry.RegistryKey;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.NamespacedKey;
import org.bukkit.entity.Chicken;


public interface ChickenOps extends Chicken, Cored {
    default int getEggLayTime() {
        return core().nbtInt("EggLayTime");
    }

    default void setEggLayTime(int time) {
        core().nbtSet("EggLayTime", "int", time);
    }

    default boolean isChickenJockey() {
        return core().nbtFlag("IsChickenJockey");
    }

    default void setIsChickenJockey(boolean jockey) {
        core().nbtSet("IsChickenJockey", "bool", jockey);
    }

    default Chicken.Variant getVariant() {
        String id = core().nbtText("variant");
        var registry = RegistryAccess.registryAccess().getRegistry(RegistryKey.CHICKEN_VARIANT);
        Chicken.Variant found = id == null ? null : registry.get(NamespacedKey.fromString(id));
        return found != null ? found : Chicken.Variant.TEMPERATE;
    }

    default Chicken.SoundVariant getSoundVariant() {
        String id = core().nbtText("sound_variant");
        var registry = RegistryAccess.registryAccess().getRegistry(RegistryKey.CHICKEN_SOUND_VARIANT);
        Chicken.SoundVariant found = id == null ? null : registry.get(NamespacedKey.fromString(id));
        return found != null ? found : Chicken.SoundVariant.CLASSIC;
    }
}
