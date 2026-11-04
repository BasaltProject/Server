package org.basaltmc.bukkit.facade;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.item.alchemy.Potion;

/** Builds a potion value from the native potion type (its effect list), resolved lazily through the holder. */
public final class PotionFactory {
    private PotionFactory() {}

    public static Holder<Potion> holder(String path) {
        return HolderRefs.of(Potion.class, Identifier.withDefaultNamespace(path));
    }

    public static Potion build(Identifier id) {
        org.bukkit.potion.PotionType type = org.bukkit.Registry.POTION.get(new org.bukkit.NamespacedKey(id.getNamespace(), id.getPath()));
        if (type == null) {
            return null;
        }
        List<MobEffectInstance> effects = new ArrayList<>();
        for (org.bukkit.potion.PotionEffect effect : type.getPotionEffects()) {
            Identifier effectId = Identifier.fromNamespaceAndPath(effect.getType().getKey().getNamespace(), effect.getType().getKey().getKey());
            Holder<MobEffect> holder = BuiltInRegistries.MOB_EFFECT.get(effectId).orElse(null);
            if (holder != null) {
                effects.add(new MobEffectInstance(holder, effect.getDuration(), effect.getAmplifier()));
            }
        }
        return new Potion(id.getPath(), effects.toArray(MobEffectInstance[]::new));
    }
}
